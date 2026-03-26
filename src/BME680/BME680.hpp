#pragma once

#include "../../include/II2CDevice.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>

// ---------------------------------------------------------------------------
// Domain types
// ---------------------------------------------------------------------------

/**
 * @brief A single compensated reading from the BME680.
 *
 * All values are in physical units - no raw ADC values exposed to the client.
 */
struct BME680Sample {
    float    temperature_c = 0.0f;  ///< Degrees Celsius
    float    pressure_hpa  = 0.0f;  ///< Hectopascals
    float    humidity_rh   = 0.0f;  ///< Relative humidity %
    uint32_t gas_ohms      = 0;     ///< Gas resistance in Ohms
    std::chrono::steady_clock::time_point timestamp{};
};

/**
 * @brief Configuration for the BME680 sensor.
 *
 * Uses physical units where possible. Safe defaults are provided so the
 * sensor can be used without any configuration.
 */
struct BME680Settings {
    uint8_t osrs_t = 4;          ///< Temperature oversampling (0=skip, 1–5 = x1/x2/x4/x8/x16)
    uint8_t osrs_p = 3;          ///< Pressure oversampling
    uint8_t osrs_h = 2;          ///< Humidity oversampling
    uint8_t filter = 2;          ///< IIR filter coefficient (0–7)

    bool enable_gas       = true; ///< Enable gas (VOC) measurement
    int  heater_temp_c    = 320;  ///< Gas heater target temperature in °C
    int  heater_time_ms   = 150;  ///< Gas heater on-time in milliseconds
    int  ambient_temp_c   = 25;   ///< Ambient temperature used in heater resistance calc
};

// ---------------------------------------------------------------------------
// BME680 driver
// ---------------------------------------------------------------------------

/**
 * @brief Driver for the Bosch BME680 environmental sensor.
 *
 * Responsibilities (Single Responsibility Principle):
 *   - Speak the BME680 register protocol.
 *   - Read and apply calibration data.
 *   - Perform forced-mode measurements and return compensated samples.
 *
 * This class does NOT own a thread, does NOT do I2C directly, and does NOT
 * know about timers or HTTP. Those concerns belong elsewhere.
 *
 * Inject any II2CDevice implementation (Linux, mock, etc.) via the constructor
 * - Dependency Inversion Principle.
 *
 * Usage:
 * @code
 *   auto dev = std::make_unique<LinuxI2CDevice>(1, 0x77);
 *   BME680 sensor(std::move(dev));
 *   sensor.initialize(BME680Settings{});
 *   BME680Sample s = sensor.readSample();
 * @endcode
 */
class BME680 {
public:
    /**
     * @param dev Concrete I2C device. Must not be null.
     */
    explicit BME680(std::unique_ptr<II2CDevice> dev);

    /**
     * @brief Soft-reset the chip, verify chip ID, read calibration, apply settings.
     * @throws std::runtime_error if chip is not found or calibration fails.
     */
    void initialize(const BME680Settings& settings);

    /**
     * @brief Update sensor settings without re-initializing.
     */
    void applySettings(const BME680Settings& settings);

    /**
     * @brief Trigger one forced measurement, wait for completion, return result.
     * @throws std::runtime_error on timeout or I2C error.
     */
    BME680Sample readSample();

private:
    // -- Registers --
    static constexpr uint8_t CHIP_ID           = 0x61;
    static constexpr uint8_t REG_CHIPID        = 0xD0;
    static constexpr uint8_t REG_SOFTRESET     = 0xE0;
    static constexpr uint8_t REG_VARIANT       = 0xF0;
    static constexpr uint8_t REG_CTRL_GAS      = 0x71;
    static constexpr uint8_t REG_CTRL_HUM      = 0x72;
    static constexpr uint8_t REG_CTRL_MEAS     = 0x74;
    static constexpr uint8_t REG_CONFIG        = 0x75;
    static constexpr uint8_t REG_MEAS_STATUS   = 0x1D;
    static constexpr uint8_t REG_COEFF1        = 0x89;
    static constexpr uint8_t REG_COEFF2        = 0xE1;
    static constexpr uint8_t REG_RES_HEAT_0    = 0x5A;
    static constexpr uint8_t REG_GAS_WAIT_0    = 0x64;
    static constexpr uint8_t MODE_FORCED       = 0x01;
    static constexpr uint8_t RUNGAS            = 0x10;
    static constexpr uint8_t VARIANT_GAS_HIGH  = 0x01;

    // -- Internal helpers --
    void     readCalibration();
    void     computeTFine(uint32_t adc_temp);
    float    temperatureC()                          const;
    float    pressureHpa(uint32_t adc_pres)          const;
    float    humidityRH(uint16_t adc_hum)            const;
    uint32_t gasOhms(uint16_t gas_adc, uint8_t gas_range) const;
    uint8_t  calcResHeat(int target_temp_c, int ambient_temp_c) const;
    static uint8_t calcGasWait(int dur_ms);
    static void    sleepMs(int ms);

    // -- State --
    std::unique_ptr<II2CDevice> dev_;
    BME680Settings settings_{};
    uint8_t  variant_    = 0;
    int32_t  t_fine_     = 0;

    // Calibration arrays (indices match standard BME680 unpack order)
    double temp_cal_[3]  = {};
    double press_cal_[10]= {};
    double hum_cal_[7]   = {};
    int    gas_cal_[3]   = {};
    uint8_t heat_range_  = 0;
    int8_t  heat_val_    = 0;
    uint8_t sw_err_      = 0;
};
