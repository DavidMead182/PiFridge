#pragma once

#include <array>
#include <cstdint>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <thread>

// Forward-declare the I2C interface (defined in demo_main.cpp)
struct II2CDevice;

// -------------------------- BME280 domain types --------------------------

struct BME280Sample {
    float temperature_c = 0.0f;
    float pressure_hpa  = 0.0f;
    float humidity_rh   = 0.0f;
    std::chrono::steady_clock::time_point timestamp;
};

struct BME280Settings {
    int i2c_bus = 1;
    uint8_t i2c_addr = 0x76; // common: 0x76 or 0x77

    // Sampling interval (event timing). No busy sleeps: timerfd blocks.
    std::chrono::milliseconds interval{1000};

    // Oversampling: 0=skipped, 1=x1, 2=x2, 3=x4, 4=x8, 5=x16
    uint8_t osrs_t = 1;
    uint8_t osrs_p = 1;
    uint8_t osrs_h = 1;

    // IIR filter: 0=off, 1=2, 2=4, 3=8, 4=16
    uint8_t filter = 0;

    // standby time (t_sb) in normal mode: 0..7 per datasheet
    uint8_t standby = 0;

    // If false, driver will run forced-mode each tick; if true, normal mode.
    bool normal_mode = true;
};

// -------------------------- BME280 sensor driver --------------------------
// SRP: this class only speaks BME280 protocol & compensation.

class BME280 {
public:
    explicit BME280(std::unique_ptr<II2CDevice> dev)
        : dev_(std::move(dev)) {}

    void initialize(const BME280Settings& s) {
        // Read chip id
        uint8_t id = 0;
        dev_->readReg(REG_ID, &id, 1);
        if (id != 0x60) {
            throw std::runtime_error("BME280 not found (chip id != 0x60)");
        }

        // Soft reset
        dev_->writeReg(REG_RESET, 0xB6);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        readCalibration();

        // Configure humidity oversampling must be written before ctrl_meas per datasheet.
        writeConfig(s);

        // Prime by reading once (updates t_fine after first temp compensation)
        (void)readSample();
    }

    void applySettings(const BME280Settings& s) { writeConfig(s); }

    BME280Sample readSample() {
        // Raw data start at 0xF7: press(3), temp(3), hum(2)
        std::array<uint8_t, 8> buf{};
        dev_->readReg(REG_PRESS_MSB, buf.data(), buf.size());

        const int32_t adc_p = (static_cast<int32_t>(buf[0]) << 12) |
                              (static_cast<int32_t>(buf[1]) << 4)  |
                              (static_cast<int32_t>(buf[2]) >> 4);

        const int32_t adc_t = (static_cast<int32_t>(buf[3]) << 12) |
                              (static_cast<int32_t>(buf[4]) << 4)  |
                              (static_cast<int32_t>(buf[5]) >> 4);

        const int32_t adc_h = (static_cast<int32_t>(buf[6]) << 8) |
                               static_cast<int32_t>(buf[7]);

        BME280Sample out;
        out.timestamp = std::chrono::steady_clock::now();
        out.temperature_c = compensateTemperatureC(adc_t);
        out.pressure_hpa  = compensatePressureHpa(adc_p);
        out.humidity_rh   = compensateHumidityRH(adc_h);
        return out;
    }

private:
    // Registers
    static constexpr uint8_t REG_ID         = 0xD0;
    static constexpr uint8_t REG_RESET      = 0xE0;
    static constexpr uint8_t REG_CTRL_HUM   = 0xF2;
    static constexpr uint8_t REG_CTRL_MEAS  = 0xF4;
    static constexpr uint8_t REG_CONFIG     = 0xF5;

    static constexpr uint8_t REG_PRESS_MSB  = 0xF7;

    // Calibration registers blocks:
    static constexpr uint8_t REG_CALIB00    = 0x88; // 0x88..0xA1 (26 bytes)
    static constexpr uint8_t REG_CALIB26    = 0xE1; // 0xE1..0xE7 (7 bytes)

    void writeConfig(const BME280Settings& s) {
        auto clamp = [](uint8_t v, uint8_t lo, uint8_t hi) -> uint8_t {
            return (v < lo) ? lo : (v > hi) ? hi : v;
        };
        const uint8_t osrs_t = clamp(s.osrs_t, 0, 5);
        const uint8_t osrs_p = clamp(s.osrs_p, 0, 5);
        const uint8_t osrs_h = clamp(s.osrs_h, 0, 5);
        const uint8_t filter = clamp(s.filter, 0, 4);
        const uint8_t t_sb   = clamp(s.standby, 0, 7);

        // ctrl_hum: [2:0] osrs_h
        dev_->writeReg(REG_CTRL_HUM, osrs_h & 0x07);

        // config: [7:5] t_sb, [4:2] filter
        const uint8_t config = static_cast<uint8_t>((t_sb << 5) | (filter << 2));
        dev_->writeReg(REG_CONFIG, config);

        // ctrl_meas: [7:5] osrs_t, [4:2] osrs_p, [1:0] mode
        const uint8_t mode = s.normal_mode ? 0x03 : 0x01; // normal vs forced
        const uint8_t ctrl_meas = static_cast<uint8_t>((osrs_t << 5) | (osrs_p << 2) | mode);
        dev_->writeReg(REG_CTRL_MEAS, ctrl_meas);
    }

    void readCalibration() {
        std::array<uint8_t, 26> c1{};
        dev_->readReg(REG_CALIB00, c1.data(), c1.size());

        dig_T1 = u16(c1[1], c1[0]);
        dig_T2 = s16(c1[3], c1[2]);
        dig_T3 = s16(c1[5], c1[4]);

        dig_P1 = u16(c1[7], c1[6]);
        dig_P2 = s16(c1[9], c1[8]);
        dig_P3 = s16(c1[11], c1[10]);
        dig_P4 = s16(c1[13], c1[12]);
        dig_P5 = s16(c1[15], c1[14]);
        dig_P6 = s16(c1[17], c1[16]);
        dig_P7 = s16(c1[19], c1[18]);
        dig_P8 = s16(c1[21], c1[20]);
        dig_P9 = s16(c1[23], c1[22]);

        dig_H1 = c1[25];

        std::array<uint8_t, 7> c2{};
        dev_->readReg(REG_CALIB26, c2.data(), c2.size());

        dig_H2 = s16(c2[1], c2[0]);
        dig_H3 = c2[2];
        dig_H4 = static_cast<int16_t>((static_cast<int16_t>(c2[3]) << 4) | (c2[4] & 0x0F));
        dig_H5 = static_cast<int16_t>((static_cast<int16_t>(c2[5]) << 4) | (c2[4] >> 4));
        dig_H6 = static_cast<int8_t>(c2[6]);
    }

    static uint16_t u16(uint8_t msb, uint8_t lsb) {
        return static_cast<uint16_t>((static_cast<uint16_t>(msb) << 8) | lsb);
    }
    static int16_t s16(uint8_t msb, uint8_t lsb) {
        return static_cast<int16_t>(static_cast<uint16_t>((static_cast<uint16_t>(msb) << 8) | lsb));
    }

    float compensateTemperatureC(int32_t adc_T) {
        int32_t var1 = ((((adc_T >> 3) - (static_cast<int32_t>(dig_T1) << 1))) * static_cast<int32_t>(dig_T2)) >> 11;
        int32_t var2 = (((((adc_T >> 4) - static_cast<int32_t>(dig_T1)) * ((adc_T >> 4) - static_cast<int32_t>(dig_T1))) >> 12) *
                        static_cast<int32_t>(dig_T3)) >> 14;
        t_fine = var1 + var2;
        float T = (t_fine * 5 + 128) / 256.0f;
        return T / 100.0f;
    }

    float compensatePressureHpa(int32_t adc_P) {
        int64_t var1 = static_cast<int64_t>(t_fine) - 128000;
        int64_t var2 = var1 * var1 * static_cast<int64_t>(dig_P6);
        var2 = var2 + ((var1 * static_cast<int64_t>(dig_P5)) << 17);
        var2 = var2 + (static_cast<int64_t>(dig_P4) << 35);
        var1 = ((var1 * var1 * static_cast<int64_t>(dig_P3)) >> 8) + ((var1 * static_cast<int64_t>(dig_P2)) << 12);
        var1 = (((static_cast<int64_t>(1) << 47) + var1) * static_cast<int64_t>(dig_P1)) >> 33;

        if (var1 == 0) return 0.0f;

        int64_t p = 1048576 - adc_P;
        p = (((p << 31) - var2) * 3125) / var1;
        var1 = (static_cast<int64_t>(dig_P9) * (p >> 13) * (p >> 13)) >> 25;
        var2 = (static_cast<int64_t>(dig_P8) * p) >> 19;
        p = ((p + var1 + var2) >> 8) + (static_cast<int64_t>(dig_P7) << 4);

        double pa = static_cast<double>(p) / 256.0;
        return static_cast<float>(pa / 100.0); // hPa
    }

    float compensateHumidityRH(int32_t adc_H) {
        int32_t v_x1_u32r = t_fine - 76800;
        v_x1_u32r = (((((adc_H << 14) - (static_cast<int32_t>(dig_H4) << 20) - (static_cast<int32_t>(dig_H5) * v_x1_u32r)) + 16384) >> 15) *
                    (((((((v_x1_u32r * static_cast<int32_t>(dig_H6)) >> 10) * (((v_x1_u32r * static_cast<int32_t>(dig_H3)) >> 11) + 32768)) >> 10) + 2097152) *
                       static_cast<int32_t>(dig_H2) + 8192) >> 14));
        v_x1_u32r = v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * static_cast<int32_t>(dig_H1)) >> 4);
        v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);
        v_x1_u32r = (v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r);
        return (v_x1_u32r >> 12) / 1024.0f;
    }

private:
    std::unique_ptr<II2CDevice> dev_;

    // Calibration coefficients
    uint16_t dig_T1{};
    int16_t  dig_T2{};
    int16_t  dig_T3{};

    uint16_t dig_P1{};
    int16_t  dig_P2{};
    int16_t  dig_P3{};
    int16_t  dig_P4{};
    int16_t  dig_P5{};
    int16_t  dig_P6{};
    int16_t  dig_P7{};
    int16_t  dig_P8{};
    int16_t  dig_P9{};

    uint8_t  dig_H1{};
    int16_t  dig_H2{};
    uint8_t  dig_H3{};
    int16_t  dig_H4{};
    int16_t  dig_H5{};
    int8_t   dig_H6{};

    int32_t  t_fine = 0;
};