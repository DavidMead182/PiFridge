#include "BME680.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <thread>
#include <vector>

// ---------------------------------------------------------------------------
// Gas resistance lookup tables (standard Bosch values)
// ---------------------------------------------------------------------------

static constexpr double LOOKUP_TABLE_1[16] = {
    2147483647.0, 2147483647.0, 2147483647.0, 2147483647.0,
    2147483647.0, 2126008810.0, 2147483647.0, 2130303777.0,
    2147483647.0, 2147483647.0, 2143188679.0, 2136746228.0,
    2147483647.0, 2126008810.0, 2147483647.0, 2147483647.0
};

static constexpr double LOOKUP_TABLE_2[16] = {
    4096000000.0, 2048000000.0, 1024000000.0, 512000000.0,
    255744255.0,  127110228.0,  64000000.0,   32258064.0,
    16016016.0,   8000000.0,    4000000.0,    2000000.0,
    1000000.0,    500000.0,     250000.0,     125000.0
};

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

BME680::BME680(std::unique_ptr<II2CDevice> dev)
    : dev_(std::move(dev)) {}

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

void BME680::initialize(const BME680Settings& settings) {
    // Soft reset
    dev_->writeReg(REG_SOFTRESET, 0xB6);
    sleepMs(10);

    // Verify chip
    uint8_t id = 0;
    dev_->readReg(REG_CHIPID, &id, 1);
    if (id != CHIP_ID)
        throw std::runtime_error("BME680 not found (expected chip ID 0x61)");

    dev_->readReg(REG_VARIANT, &variant_, 1);

    readCalibration();
    applySettings(settings);
}

void BME680::applySettings(const BME680Settings& s) {
    settings_ = s;

    // IIR filter
    dev_->writeReg(REG_CONFIG, static_cast<uint8_t>((s.filter & 0x07) << 2));

    // Oversampling (mode bits left at 0 = sleep; forced set on each readSample)
    const uint8_t ctrl_meas = static_cast<uint8_t>(
        ((s.osrs_t & 0x07) << 5) | ((s.osrs_p & 0x07) << 2));
    dev_->writeReg(REG_CTRL_MEAS, ctrl_meas);

    dev_->writeReg(REG_CTRL_HUM, static_cast<uint8_t>(s.osrs_h & 0x07));

    // Gas heater (slot 0)
    if (s.enable_gas) {
        dev_->writeReg(REG_RES_HEAT_0, calcResHeat(s.heater_temp_c, s.ambient_temp_c));
        dev_->writeReg(REG_GAS_WAIT_0, calcGasWait(s.heater_time_ms));

        const uint8_t run_gas = (variant_ == VARIANT_GAS_HIGH)
                                ? static_cast<uint8_t>(RUNGAS << 1)
                                : RUNGAS;
        dev_->writeReg(REG_CTRL_GAS, run_gas);
    } else {
        dev_->writeReg(REG_CTRL_GAS, 0x00);
    }
}

BME680Sample BME680::readSample() {
    // Trigger forced mode
    uint8_t ctrl = 0;
    dev_->readReg(REG_CTRL_MEAS, &ctrl, 1);
    dev_->writeReg(REG_CTRL_MEAS, static_cast<uint8_t>((ctrl & 0xFC) | MODE_FORCED));

    // Poll for new data (bit 7 of status register), max ~3 s
    std::array<uint8_t, 17> data{};
    bool got_data = false;
    for (int tries = 0; tries < 600; ++tries) {
        dev_->readReg(REG_MEAS_STATUS, data.data(), data.size());
        if (data[0] & 0x80u) { got_data = true; break; }
        sleepMs(5);
    }
    if (!got_data)
        throw std::runtime_error("BME680: timeout waiting for measurement");

    // Parse raw ADC values from burst read
    const uint32_t adc_pres = ((uint32_t(data[2]) << 16) | (uint32_t(data[3]) << 8) | data[4]) >> 4;
    const uint32_t adc_temp = ((uint32_t(data[5]) << 16) | (uint32_t(data[6]) << 8) | data[7]) >> 4;
    const uint16_t adc_hum  = static_cast<uint16_t>((uint16_t(data[8]) << 8) | data[9]);

    uint16_t gas_adc  = 0;
    uint8_t  gas_range = 0;
    if (variant_ == VARIANT_GAS_HIGH) {
        gas_adc   = static_cast<uint16_t>(((uint16_t(data[15]) << 8) | data[16]) >> 6);
        gas_range = data[16] & 0x0F;
    } else {
        gas_adc   = static_cast<uint16_t>(((uint16_t(data[13]) << 8) | data[14]) >> 6);
        gas_range = data[14] & 0x0F;
    }

    // Temperature must be computed first - it sets t_fine_ used by the others
    computeTFine(adc_temp);

    BME680Sample out;
    out.timestamp     = std::chrono::steady_clock::now();
    out.temperature_c = temperatureC();
    out.pressure_hpa  = pressureHpa(adc_pres);
    out.humidity_rh   = humidityRH(adc_hum);
    out.gas_ohms      = settings_.enable_gas ? gasOhms(gas_adc, gas_range) : 0;

    return out;
}

// ---------------------------------------------------------------------------
// Private: calibration
// ---------------------------------------------------------------------------

void BME680::readCalibration() {
    std::array<uint8_t, 25> c1{};
    std::array<uint8_t, 16> c2{};
    dev_->readReg(REG_COEFF1, c1.data(), c1.size());
    dev_->readReg(REG_COEFF2, c2.data(), c2.size());

    // Merge into one block and skip byte 0 (matches standard unpack offset)
    std::array<uint8_t, 41> coeff{};
    std::copy(c1.begin(), c1.end(), coeff.begin());
    std::copy(c2.begin(), c2.end(), coeff.begin() + 25);
    const uint8_t* b = coeff.data() + 1; // 38 usable bytes

    // Helper lambdas to read little-endian values sequentially
    size_t i = 0;
    auto i16 = [&]() -> double { double v = double(int16_t(uint16_t(b[i]) | uint16_t(b[i+1])<<8)); i+=2; return v; };
    auto u16 = [&]() -> double { double v = double(uint16_t(b[i]) | uint16_t(b[i+1])<<8);          i+=2; return v; };
    auto i8  = [&]() -> double { return double(int8_t(b[i++])); };
    auto u8  = [&]() -> double { return double(b[i++]); };

    // Unpack format: "<hbBHhbBhhbbHhhBBBHbbbBbHhbb"
    std::array<double, 27> v{};
    v[0]  = i16(); v[1]  = i8();  v[2]  = u8();  v[3]  = u16();
    v[4]  = i16(); v[5]  = i8();  v[6]  = u8();  v[7]  = i16();
    v[8]  = i16(); v[9]  = i8();  v[10] = i8();  v[11] = u16();
    v[12] = i16(); v[13] = i16(); v[14] = u8();  v[15] = u8();
    v[16] = u8();  v[17] = u16(); v[18] = i8();  v[19] = i8();
    v[20] = i8();  v[21] = u8();  v[22] = i8();  v[23] = u16();
    v[24] = i16(); v[25] = i8();  v[26] = i8();

    temp_cal_[0] = v[23]; temp_cal_[1] = v[0]; temp_cal_[2] = v[1];

    press_cal_[0] = v[3];  press_cal_[1] = v[4];  press_cal_[2] = v[5];
    press_cal_[3] = v[7];  press_cal_[4] = v[8];  press_cal_[5] = v[10];
    press_cal_[6] = v[9];  press_cal_[7] = v[12]; press_cal_[8] = v[13];
    press_cal_[9] = v[14];

    hum_cal_[0] = v[17]; hum_cal_[1] = v[16]; hum_cal_[2] = v[18];
    hum_cal_[3] = v[19]; hum_cal_[4] = v[20]; hum_cal_[5] = v[21];
    hum_cal_[6] = v[22];
    // H1/H2 fixup
    hum_cal_[1] = hum_cal_[1] * 16.0 + std::fmod(hum_cal_[0], 16.0);
    hum_cal_[0] /= 16.0;

    gas_cal_[0] = int(v[25]); gas_cal_[1] = int(v[24]); gas_cal_[2] = int(v[26]);

    uint8_t tmp = 0;
    dev_->readReg(0x02, &tmp, 1);
    heat_range_ = (tmp & 0x30u) >> 4;
    dev_->readReg(0x00, reinterpret_cast<uint8_t*>(&heat_val_), 1);
    dev_->readReg(0x04, &tmp, 1);
    sw_err_ = (tmp & 0xF0u) >> 4;
}

// ---------------------------------------------------------------------------
// Private: compensation formulas
// ---------------------------------------------------------------------------

void BME680::computeTFine(uint32_t adc_temp) {
    const double var1 = (double(adc_temp) / 8.0) - (temp_cal_[0] * 2.0);
    const double var2 = (var1 * temp_cal_[1]) / 2048.0;
    const double var3 = ((var1 / 2.0) * (var1 / 2.0) / 4096.0) * (temp_cal_[2] * 16.0) / 16384.0;
    t_fine_ = int32_t(var2 + var3);
}

float BME680::temperatureC() const {
    return float(((double(t_fine_) * 5.0) + 128.0) / 256.0 / 100.0);
}

float BME680::pressureHpa(uint32_t adc_pres) const {
    double var1 = double(t_fine_) / 2.0 - 64000.0;
    double var2 = ((var1 / 4.0) * (var1 / 4.0) / 2048.0) * press_cal_[5] / 4.0
                + var1 * press_cal_[4] * 2.0;
    var2 = var2 / 4.0 + press_cal_[3] * 65536.0;
    var1 = (((var1 / 4.0) * (var1 / 4.0) / 8192.0) * (press_cal_[2] * 32.0) / 8.0
           + press_cal_[1] * var1 / 2.0) / 262144.0;
    var1 = (32768.0 + var1) * press_cal_[0] / 32768.0;

    double calc = (1048576.0 - double(adc_pres) - var2 / 4096.0) * 3125.0 / var1 * 2.0;

    var1 = press_cal_[8] * ((calc / 8.0) * (calc / 8.0) / 8192.0) / 4096.0;
    var2 = (calc / 4.0) * press_cal_[7] / 8192.0;
    const double var3 = std::pow(calc / 256.0, 3.0) * press_cal_[9] / 131072.0;

    calc += (var1 + var2 + var3 + press_cal_[6] * 128.0) / 16.0;
    return float(calc / 100.0); // Pa -> hPa
}

float BME680::humidityRH(uint16_t adc_hum) const {
    const double temp_scaled = (double(t_fine_) * 5.0 + 128.0) / 256.0;
    double var1 = double(adc_hum) - hum_cal_[0] * 16.0 - (temp_scaled * hum_cal_[2] / 200.0);
    double var2 = hum_cal_[1] * (temp_scaled * hum_cal_[3] / 100.0
                + (temp_scaled * (temp_scaled * hum_cal_[4] / 100.0) / 64.0) / 100.0
                + 16384.0) / 1024.0;
    double var3 = var1 * var2;
    double var4 = (hum_cal_[5] * 128.0 + temp_scaled * hum_cal_[6] / 100.0) / 16.0;
    double var5 = (var3 / 16384.0) * (var3 / 16384.0) / 1024.0;
    double calc = (var3 + var4 * var5 / 2.0) / 1024.0 * 1000.0 / 4096.0 / 1000.0;
    return float(std::clamp(calc, 0.0, 100.0));
}

uint32_t BME680::gasOhms(uint16_t gas_adc, uint8_t gas_range) const {
    gas_range &= 0x0F;
    if (variant_ == VARIANT_GAS_HIGH) {
        const double var1 = double(262144u >> gas_range);
        const double var2 = double(int(gas_adc) - 512) * 3.0 + 4096.0;
        return uint32_t((1000.0 * var1 / var2) * 100.0);
    }
    const double var1 = (double(1340 + 5 * int(sw_err_)) * LOOKUP_TABLE_1[gas_range]) / 65536.0;
    const double var2 = (double(gas_adc) * 32768.0 - 16777216.0) + var1;
    const double var3 = LOOKUP_TABLE_2[gas_range] * var1 / 512.0;
    return uint32_t((var3 + var2 / 2.0) / var2);
}

uint8_t BME680::calcResHeat(int target_temp_c, int ambient_temp_c) const {
    const int temp   = std::min(target_temp_c, 400);
    const int32_t v1 = int32_t((ambient_temp_c * gas_cal_[2]) / 1000) * 256;
    const int32_t v2 = int32_t((gas_cal_[0] + 784) *
                       ((((gas_cal_[1] + 154009) * temp * 5) / 100 + 3276800) / 10));
    const int32_t v3 = v1 + v2 / 2;
    const int32_t v4 = v3 / (int(heat_range_) + 4);
    const int32_t v5 = 131 * int(heat_val_) + 65536;
    const int32_t res_x100 = int32_t(((v4 * 100) / v5) - 250) * 34;
    return uint8_t(std::clamp<int32_t>((res_x100 + 50) / 100, 0, 255));
}

uint8_t BME680::calcGasWait(int dur_ms) {
    if (dur_ms >= 0xFC0) return 0xFF;
    int factor = 0, dur = dur_ms;
    while (dur > 0x3F) { dur /= 4; ++factor; }
    return uint8_t(std::clamp(dur + factor * 64, 0, 255));
}

void BME680::sleepMs(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}
