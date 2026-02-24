#pragma once

#include <array>
#include <cstdint>
#include <chrono>
#include <functional>
#include <memory>
#include <stdexcept>
#include <vector>
#include <algorithm>

// -------------------------- I2C interface (dependency) --------------------------
// High-level BME680 depends on this interface, not on Linux specifics.

struct II2CDevice {
    virtual ~II2CDevice() = default;
    virtual void writeBytes(const uint8_t* data, size_t len) = 0;
    virtual void readBytes(uint8_t* data, size_t len) = 0;

    virtual void writeReg(uint8_t reg, uint8_t value) = 0;
    virtual void readReg(uint8_t reg, uint8_t* data, size_t len) = 0;
};

// -------------------------- Domain types --------------------------

struct BME680Sample {
    float temperature_c = 0.0f;
    float pressure_hpa  = 0.0f;
    float humidity_rh   = 0.0f;
    uint32_t gas_ohms    = 0;
    std::chrono::steady_clock::time_point timestamp{};
};

struct BME680Settings {
    // Timing outside the driver (thread/timerfd) decides when to call readSample().
    // The driver itself performs a forced conversion + polling for completion.
    uint8_t osrs_t = 4;   // 0=skip, 1=x1, 2=x2, 3=x4, 4=x8, 5=x16
    uint8_t osrs_p = 3;
    uint8_t osrs_h = 2;
    uint8_t filter = 2;   // IIR filter setting (0..7 typical in BME680 libs)

    // Gas heater
    bool enable_gas = true;
    int heater_temp_c = 320;     // typical default used by many libs
    int heater_time_ms = 150;    // typical default used by many libs
    int ambient_temp_c = 25;     // used in heater resistance calc
};

// -------------------------- BME680 driver --------------------------
// SRP: talks BME680 protocol, calibration, compensation, heater setup.

class BME680 {
public:
    explicit BME680(std::unique_ptr<II2CDevice> dev)
        : dev_(std::move(dev)) {}

    void initialize(const BME680Settings& s) {
        // Soft reset then check chip ID.
        dev_->writeReg(REG_SOFTRESET, 0xB6);

        // Chip ID is 0x61 for BME680.
        uint8_t id = 0;
        dev_->readReg(REG_CHIPID, &id, 1);
        if (id != CHIP_ID) {
            throw std::runtime_error("BME680 not found (chip id != 0x61)");
        }

        // Variant (affects gas register layout + control bits).
        dev_->readReg(REG_VARIANT, &variant_, 1);

        readCalibration();

        // Apply initial config (oversampling/filter + heater/gas enable).
        applySettings(s);
    }

    void applySettings(const BME680Settings& s) {
        settings_ = s;

        // Filter (REG_CONFIG): bits [4:2] in many BME680 implementations (value << 2)
        // (Matches Adafruit approach.)
        dev_->writeReg(REG_CONFIG, static_cast<uint8_t>((settings_.filter & 0x07) << 2));

        // ctrl_meas: (osrs_t<<5) | (osrs_p<<2) | mode
        // We write osrs bits now; mode gets set to forced when we trigger a read.
        uint8_t ctrl_meas = static_cast<uint8_t>(((settings_.osrs_t & 0x07) << 5) |
                                                 ((settings_.osrs_p & 0x07) << 2));
        dev_->writeReg(REG_CTRL_MEAS, ctrl_meas);

        // ctrl_hum: osrs_h
        dev_->writeReg(REG_CTRL_HUM, static_cast<uint8_t>(settings_.osrs_h & 0x07));

        // Heater + gas config (slot 0)
        if (settings_.enable_gas) {
            const uint8_t rh = calcResHeat(settings_.heater_temp_c, settings_.ambient_temp_c);
            const uint8_t gw = calcGasWait(settings_.heater_time_ms);
            dev_->writeReg(REG_RES_HEAT_0, rh);
            dev_->writeReg(REG_GAS_WAIT_0, gw);

            // Enable run gas (variant-dependent shift in some implementations)
            uint8_t run_gas = RUNGAS;
            if (variant_ == VARIANT_GAS_HIGH) {
                // Some variants use different bit positioning; Adafruit shifts by 1.
                run_gas = static_cast<uint8_t>(RUNGAS << 1);
            }
            dev_->writeReg(REG_CTRL_GAS, run_gas);
        } else {
            dev_->writeReg(REG_CTRL_GAS, 0x00);
        }
    }

    // Performs a forced measurement, waits for completion, returns compensated values.
    BME680Sample readSample() {
        // Trigger forced mode: set mode bits [1:0] = 0b01
        uint8_t ctrl = 0;
        dev_->readReg(REG_CTRL_MEAS, &ctrl, 1);
        ctrl = static_cast<uint8_t>((ctrl & 0xFC) | MODE_FORCED);
        dev_->writeReg(REG_CTRL_MEAS, ctrl);

        // Poll for "new data" in MEAS_STATUS (0x1D), bit 7 set.
        // Read 17 bytes starting at 0x1D (matches common BME680 register burst layouts).
        std::array<uint8_t, 17> data{};
        for (int tries = 0; tries < 600; ++tries) { // up to ~3s if caller sleeps 5ms between calls
            dev_->readReg(REG_MEAS_STATUS, data.data(), data.size());
            const bool new_data = (data[0] & 0x80u) != 0u;
            if (new_data) break;
            // Busy wait is avoided in the outer app; but we still need a small delay here.
            // (demo_main uses forced reads at >= 1Hz; this is fine.)
            sleepMs(5);
            if (tries == 599) throw std::runtime_error("Timeout while reading BME680 data");
        }

        // Raw parsing (matches Adafruit’s layout).
        const uint32_t adc_pres = read24(data[2], data[3], data[4]) >> 4; // /16
        const uint32_t adc_temp = read24(data[5], data[6], data[7]) >> 4; // /16
        const uint16_t adc_hum  = static_cast<uint16_t>((uint16_t(data[8]) << 8) | data[9]);

        uint16_t gas_adc = 0;
        uint8_t gas_range = 0;
        if (variant_ == VARIANT_GAS_HIGH) {
            gas_adc   = static_cast<uint16_t>((uint16_t(data[15]) << 8) | data[16]);
            gas_adc   = static_cast<uint16_t>(gas_adc >> 6); // /64
            gas_range = static_cast<uint8_t>(data[16] & 0x0F);
        } else {
            gas_adc   = static_cast<uint16_t>((uint16_t(data[13]) << 8) | data[14]);
            gas_adc   = static_cast<uint16_t>(gas_adc >> 6); // /64
            gas_range = static_cast<uint8_t>(data[14] & 0x0F);
        }

        // Compensation (ported directly from well-known BME680 integer/float style formulas)
        // Temperature first -> t_fine
        computeTFine(adc_temp);

        BME680Sample out;
        out.timestamp      = std::chrono::steady_clock::now();
        out.temperature_c  = temperatureC();
        out.pressure_hpa   = pressureHpa(adc_pres);
        out.humidity_rh    = humidityRH(adc_hum);

        if (settings_.enable_gas) {
            out.gas_ohms = gasOhms(gas_adc, gas_range);
        } else {
            out.gas_ohms = 0;
        }

        return out;
    }

private:
    // -------------------------- Registers / constants --------------------------
    static constexpr uint8_t CHIP_ID = 0x61;

    static constexpr uint8_t REG_CHIPID     = 0xD0;
    static constexpr uint8_t REG_SOFTRESET  = 0xE0;
    static constexpr uint8_t REG_VARIANT    = 0xF0;

    static constexpr uint8_t REG_CTRL_GAS   = 0x71;
    static constexpr uint8_t REG_CTRL_HUM   = 0x72;
    static constexpr uint8_t REG_CTRL_MEAS  = 0x74;
    static constexpr uint8_t REG_CONFIG     = 0x75;

    static constexpr uint8_t REG_MEAS_STATUS = 0x1D; // burst read base

    static constexpr uint8_t REG_COEFF1     = 0x89; // read 25 bytes
    static constexpr uint8_t REG_COEFF2     = 0xE1; // read 16 bytes

    static constexpr uint8_t REG_RES_HEAT_0 = 0x5A;
    static constexpr uint8_t REG_GAS_WAIT_0 = 0x64;

    static constexpr uint8_t MODE_FORCED    = 0x01;
    static constexpr uint8_t RUNGAS         = 0x10;

    static constexpr uint8_t VARIANT_GAS_HIGH = 0x01;

    // Gas lookup tables (from common BME680 implementations)
    static constexpr std::array<double, 16> LOOKUP_TABLE_1 = {
        2147483647.0, 2147483647.0, 2147483647.0, 2147483647.0,
        2147483647.0, 2126008810.0, 2147483647.0, 2130303777.0,
        2147483647.0, 2147483647.0, 2143188679.0, 2136746228.0,
        2147483647.0, 2126008810.0, 2147483647.0, 2147483647.0
    };

    static constexpr std::array<double, 16> LOOKUP_TABLE_2 = {
        4096000000.0, 2048000000.0, 1024000000.0, 512000000.0,
        255744255.0, 127110228.0, 64000000.0,   32258064.0,
        16016016.0,   8000000.0,    4000000.0,  2000000.0,
        1000000.0,    500000.0,     250000.0,   125000.0
    };

    // -------------------------- Calibration storage --------------------------
    // These “packed” coeff vectors mirror the indexing used by Adafruit’s unpack.
    // temp_cal: [T1, T2, T3] via indices [23,0,1]
    // press_cal: 10 coeffs via [3,4,5,7,8,10,9,12,13,14]
    // hum_cal: 7 coeffs via [17,16,18,19,20,21,22] then H1/H2 fixup
    // gas_cal: [GH1, GH2, GH3] via [25,24,26]
    std::array<double, 3>  temp_cal_{};
    std::array<double, 10> press_cal_{};
    std::array<double, 7>  hum_cal_{};
    std::array<int, 3>     gas_cal_{};

    uint8_t heat_range_ = 0;
    int8_t  heat_val_   = 0;
    uint8_t sw_err_     = 0;

    uint8_t variant_    = 0;

    int32_t t_fine_     = 0;
    BME680Settings settings_{};

    std::unique_ptr<II2CDevice> dev_;

private:
    static void sleepMs(int ms);

    static uint32_t read24(uint8_t b0, uint8_t b1, uint8_t b2) {
        return (uint32_t(b0) << 16) | (uint32_t(b1) << 8) | uint32_t(b2);
    }

    static int16_t le_i16(const uint8_t* p) {
        return int16_t(uint16_t(p[0]) | (uint16_t(p[1]) << 8));
    }
    static uint16_t le_u16(const uint8_t* p) {
        return uint16_t(p[0]) | (uint16_t(p[1]) << 8);
    }

    // Unpack the exact type stream used by:
    // struct.unpack("<hbBHhbBhhbbHhhBBBHbbbBbHhbb", bytes(coeff[1:39]))
    static std::vector<int32_t> unpackCoeffs(const uint8_t* bytes38);

    void readCalibration() {
        std::array<uint8_t, 25> c1{};
        std::array<uint8_t, 16> c2{};
        dev_->readReg(REG_COEFF1, c1.data(), c1.size());
        dev_->readReg(REG_COEFF2, c2.data(), c2.size());

        std::array<uint8_t, 41> coeff{};
        std::copy(c1.begin(), c1.end(), coeff.begin());
        std::copy(c2.begin(), c2.end(), coeff.begin() + c1.size());

        // Use bytes [1:39] (38 bytes)
        const uint8_t* p = coeff.data() + 1;
        auto vals = unpackCoeffs(p);
        if (vals.size() < 27) throw std::runtime_error("Calibration unpack failed");

        // Convert to double for formula consistency with reference code.
        std::vector<double> v(vals.size());
        for (size_t i = 0; i < vals.size(); ++i) v[i] = double(vals[i]);

        temp_cal_ = { v[23], v[0], v[1] };

        press_cal_ = { v[3], v[4], v[5], v[7], v[8], v[10], v[9], v[12], v[13], v[14] };

        hum_cal_ = { v[17], v[16], v[18], v[19], v[20], v[21], v[22] };
        // flip around H1 & H2 as in reference
        hum_cal_[1] *= 16.0;
        hum_cal_[1] += std::fmod(hum_cal_[0], 16.0);
        hum_cal_[0] /= 16.0;

        gas_cal_ = { int(vals[25]), int(vals[24]), int(vals[26]) };

        // heater range/val & sw err (same registers used by common drivers)
        uint8_t tmp = 0;
        dev_->readReg(0x02, &tmp, 1);
        heat_range_ = uint8_t((tmp & 0x30u) >> 4);

        dev_->readReg(0x00, reinterpret_cast<uint8_t*>(&heat_val_), 1);

        dev_->readReg(0x04, &tmp, 1);
        sw_err_ = uint8_t((tmp & 0xF0u) >> 4);
    }

    void computeTFine(uint32_t adc_temp) {
        // Matches common BME680 integer-ish approach:
        const double var1 = (double(adc_temp) / 8.0) - (temp_cal_[0] * 2.0);
        const double var2 = (var1 * temp_cal_[1]) / 2048.0;
        double var3 = ((var1 / 2.0) * (var1 / 2.0)) / 4096.0;
        var3 = (var3 * temp_cal_[2] * 16.0) / 16384.0;
        t_fine_ = int32_t(var2 + var3);
    }

    float temperatureC() const {
        const double calc_temp = ((double(t_fine_) * 5.0) + 128.0) / 256.0;
        return float(calc_temp / 100.0);
    }

    float pressureHpa(uint32_t adc_pres) const {
        // Matches common BME680 pressure compensation formula
        double var1 = (double(t_fine_) / 2.0) - 64000.0;
        double var2 = ((var1 / 4.0) * (var1 / 4.0)) / 2048.0;
        var2 = (var2 * press_cal_[5]) / 4.0;
        var2 = var2 + (var1 * press_cal_[4] * 2.0);
        var2 = (var2 / 4.0) + (press_cal_[3] * 65536.0);
        var1 = (((var1 / 4.0) * (var1 / 4.0)) / 8192.0) * (press_cal_[2] * 32.0) / 8.0
               + ((press_cal_[1] * var1) / 2.0);
        var1 = var1 / 262144.0;
        var1 = ((32768.0 + var1) * press_cal_[0]) / 32768.0;

        double calc_pres = 1048576.0 - double(adc_pres);
        calc_pres = (calc_pres - (var2 / 4096.0)) * 3125.0;
        calc_pres = (calc_pres / var1) * 2.0;

        var1 = (press_cal_[8] * (((calc_pres / 8.0) * (calc_pres / 8.0)) / 8192.0)) / 4096.0;
        var2 = ((calc_pres / 4.0) * press_cal_[7]) / 8192.0;
        const double var3 = (std::pow((calc_pres / 256.0), 3.0) * press_cal_[9]) / 131072.0;

        calc_pres += (var1 + var2 + var3 + (press_cal_[6] * 128.0)) / 16.0;

        // Pa -> hPa
        return float(calc_pres / 100.0);
    }

    float humidityRH(uint16_t adc_hum) const {
        // Matches common BME680 humidity compensation formula
        const double temp_scaled = ((double(t_fine_) * 5.0) + 128.0) / 256.0;

        double var1 = (double(adc_hum) - (hum_cal_[0] * 16.0)) - ((temp_scaled * hum_cal_[2]) / 200.0);
        double var2 = (hum_cal_[1] *
                        ((temp_scaled * hum_cal_[3]) / 100.0 +
                         ((temp_scaled * (temp_scaled * hum_cal_[4]) / 100.0) / 64.0) / 100.0 +
                         16384.0)) / 1024.0;
        double var3 = var1 * var2;
        double var4 = hum_cal_[5] * 128.0;
        var4 = (var4 + ((temp_scaled * hum_cal_[6]) / 100.0)) / 16.0;
        double var5 = ((var3 / 16384.0) * (var3 / 16384.0)) / 1024.0;
        double var6 = (var4 * var5) / 2.0;

        double calc_hum = (((var3 + var6) / 1024.0) * 1000.0) / 4096.0;
        calc_hum /= 1000.0;

        calc_hum = std::min(calc_hum, 100.0);
        calc_hum = std::max(calc_hum, 0.0);
        return float(calc_hum);
    }

    uint32_t gasOhms(uint16_t gas_adc, uint8_t gas_range) const {
        gas_range &= 0x0F;

        if (variant_ == VARIANT_GAS_HIGH) {
            // Variant-high formula used by some BME68x parts (kept for compatibility).
            // Matches reference snippet used in common libs.
            const double var1 = double(262144u >> gas_range);
            double var2 = double(int(gas_adc) - 512);
            var2 *= 3.0;
            var2 = 4096.0 + var2;
            double res = (1000.0 * var1) / var2;
            res *= 100.0;
            return uint32_t(res);
        } else {
            // Classic BME680 formula with lookup tables + sw_err
            const double var1 = (double(1340 + (5 * int(sw_err_))) * LOOKUP_TABLE_1[gas_range]) / 65536.0;
            const double var2 = ((double(gas_adc) * 32768.0) - 16777216.0) + var1;
            const double var3 = (LOOKUP_TABLE_2[gas_range] * var1) / 512.0;
            const double res  = (var3 + (var2 / 2.0)) / var2;
            return uint32_t(res);
        }
    }

    uint8_t calcResHeat(int target_temp_c, int ambient_temp_c) const {
        // Integer-ish version (matches common BME68x/BME680 heater calc found in libs)
        const int gh1 = gas_cal_[0];
        const int gh2 = gas_cal_[1];
        const int gh3 = gas_cal_[2];

        const int htr = int(heat_range_);
        const int htv = int(heat_val_);
        const int amb = ambient_temp_c;

        int temp = std::min(target_temp_c, 400);

        const int32_t var1 = int32_t(((int32_t(amb) * gh3) / 1000) * 256);
        const int32_t var2 = int32_t((gh1 + 784) * (((((gh2 + 154009) * temp * 5) / 100) + 3276800) / 10));
        const int32_t var3 = var1 + (var2 / 2);
        const int32_t var4 = var3 / (htr + 4);
        const int32_t var5 = (131 * htv) + 65536;
        const int32_t heatr_res_x100 = int32_t(((var4 * 100) / var5) - 250) * 34;
        const int32_t heatr_res = (heatr_res_x100 + 50) / 100;

        return uint8_t(std::clamp<int32_t>(heatr_res, 0, 255));
    }

    static uint8_t calcGasWait(int dur_ms) {
        // Encode heater on-time (gas_wait) as per common BME680 helper.
        int factor = 0;
        int dur = dur_ms;

        if (dur >= 0xFC0) return 0xFF;

        while (dur > 0x3F) {
            dur /= 4;
            factor++;
        }
        const int durval = dur + (factor * 64);
        return uint8_t(std::clamp(durval, 0, 255));
    }
};