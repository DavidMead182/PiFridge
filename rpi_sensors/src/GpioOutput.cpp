#include "GpioOutput.hpp"

#include <stdexcept>

#include <gpiod.h>

GpioOutput::GpioOutput(std::string chipName, unsigned int lineOffset, bool activeHigh)
    : chip_(nullptr),
      line_(nullptr),
      activeHigh_(activeHigh) {

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

    int defaultValue = activeHigh_ ? 0 : 1;
    if (gpiod_line_request_output(line_, "PiFridge", defaultValue) != 0) {
        gpiod_chip_close(chip_);
        chip_ = nullptr;
        line_ = nullptr;
        throw std::runtime_error("GpioOutput request output failed");
    }
}

GpioOutput::~GpioOutput() {
    if (line_) {
        gpiod_line_release(line_);
        line_ = nullptr;
    }
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
    if (!line_) {
        throw std::runtime_error("GpioOutput line not ready");
    }
    if (gpiod_line_set_value(line_, value) != 0) {
        throw std::runtime_error("GpioOutput set value failed");
    }
}
