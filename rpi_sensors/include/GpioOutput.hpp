#pragma once

#include <string>

#include "IGpioOutput.hpp"

struct gpiod_chip;
struct gpiod_line;

class GpioOutput final : public IGpioOutput {
public:
    explicit GpioOutput(std::string chipName = "gpiochip0",
                        unsigned int lineOffset = 17,
                        bool activeHigh = true);

    ~GpioOutput() override;

    GpioOutput(const GpioOutput&) = delete;
    GpioOutput& operator=(const GpioOutput&) = delete;

    void setHigh() override;
    void setLow() override;

private:
    void setValue(int value);

    gpiod_chip* chip_;
    gpiod_line* line_;
    bool activeHigh_;
};
