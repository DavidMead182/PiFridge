#include "GpioOutput.hpp"

#include <stdexcept>
#include <string>

#include <gpiod.h>

namespace {

#if defined(PIFRIDGE_GPIOD_V2) && PIFRIDGE_GPIOD_V2
std::string normalizeChipPath(const std::string& chipName) {
    // libgpiod v2 expects a device path like "/dev/gpiochip0"
    if (chipName.rfind("/dev/", 0) == 0) {
        return chipName;
    }
    return "/dev/" + chipName;
}

gpiod_line_value toLineValue(int v) {
    return v ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE;
}
#endif

} // namespace

GpioOutput::GpioOutput(std::string chipName, unsigned int lineOffset, bool activeHigh)
    : chip_(nullptr),
#if defined(PIFRIDGE_GPIOD_V2) && PIFRIDGE_GPIOD_V2
      request_(nullptr),
      lineOffset_(lineOffset),
#else
      line_(nullptr),
#endif
      activeHigh_(activeHigh) {

#if defined(PIFRIDGE_GPIOD_V2) && PIFRIDGE_GPIOD_V2
    const std::string chipPath = normalizeChipPath(chipName);

    chip_ = gpiod_chip_open(chipPath.c_str());
    if (!chip_) {
        throw std::runtime_error("GpioOutput open chip failed");
    }

    gpiod_line_settings* settings = gpiod_line_settings_new();
    gpiod_line_config* lineCfg = gpiod_line_config_new();
    gpiod_request_config* reqCfg = gpiod_request_config_new();

    if (!settings || !lineCfg || !reqCfg) {
        if (settings) gpiod_line_settings_free(settings);
        if (lineCfg) gpiod_line_config_free(lineCfg);
        if (reqCfg) gpiod_request_config_free(reqCfg);
        gpiod_chip_close(chip_);
        chip_ = nullptr;
        throw std::runtime_error("GpioOutput libgpiod alloc failed");
    }

    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);

    const int defaultValue = activeHigh_ ? 0 : 1;
    gpiod_line_settings_set_output_value(settings, toLineValue(defaultValue));

    const unsigned int offsets[1] = { lineOffset_ };
    if (gpiod_line_config_add_line_settings(lineCfg, offsets, 1, settings) != 0) {
        gpiod_line_settings_free(settings);
        gpiod_line_config_free(lineCfg);
        gpiod_request_config_free(reqCfg);
        gpiod_chip_close(chip_);
        chip_ = nullptr;
        throw std::runtime_error("GpioOutput configure line failed");
    }

    gpiod_request_config_set_consumer(reqCfg, "PiFridge");

    request_ = gpiod_chip_request_lines(chip_, reqCfg, lineCfg);

    gpiod_line_settings_free(settings);
    gpiod_line_config_free(lineCfg);
    gpiod_request_config_free(reqCfg);

    if (!request_) {
        gpiod_chip_close(chip_);
        chip_ = nullptr;
        throw std::runtime_error("GpioOutput request lines failed");
    }

#else
    chip_ = gpiod_chip_open_by_name(chipName.c_str());
    if (!chip_) {
        throw std::runtime_error("GpioOutput open chip failed");
    }

    line_ = gpiod_chip_get_line(chip_, lineOffset);
    if (!line_) {
        gpiod_chip_close(chip_);
        chip_ = nullptr;
        throw std::runtime_error("GpioOutput get line failed");
    }

    const int defaultValue = activeHigh_ ? 0 : 1;
    if (gpiod_line_request_output(line_, "PiFridge", defaultValue) != 0) {
        gpiod_chip_close(chip_);
        chip_ = nullptr;
        line_ = nullptr;
        throw std::runtime_error("GpioOutput request output failed");
    }
#endif
}

GpioOutput::~GpioOutput() {
#if defined(PIFRIDGE_GPIOD_V2) && PIFRIDGE_GPIOD_V2
    if (request_) {
        gpiod_line_request_release(request_);
        request_ = nullptr;
    }
#else
    if (line_) {
        gpiod_line_release(line_);
        line_ = nullptr;
    }
#endif

    if (chip_) {
        gpiod_chip_close(chip_);
        chip_ = nullptr;
    }
}

void GpioOutput::setHigh() {
    setValue(activeHigh_ ? 1 : 0);
}

void GpioOutput::setLow() {
    setValue(activeHigh_ ? 0 : 1);
}

void GpioOutput::setValue(int value) {
#if defined(PIFRIDGE_GPIOD_V2) && PIFRIDGE_GPIOD_V2
    if (!request_) {
        throw std::runtime_error("GpioOutput request not ready");
    }
    if (gpiod_line_request_set_value(request_, lineOffset_, toLineValue(value)) != 0) {
        throw std::runtime_error("GpioOutput set value failed");
    }
#else
    if (!line_) {
        throw std::runtime_error("GpioOutput line not ready");
    }
    if (gpiod_line_set_value(line_, value) != 0) {
        throw std::runtime_error("GpioOutput set value failed");
    }
#endif
}
