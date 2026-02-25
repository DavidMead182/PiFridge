#pragma once

#include <string>

#include "IGpioOutput.hpp"

struct gpiod_chip;

#if defined(PIFRIDGE_GPIOD_V2) && PIFRIDGE_GPIOD_V2
struct gpiod_line_request;
#else
struct gpiod_line;
#endif

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

#if defined(PIFRIDGE_GPIOD_V2) && PIFRIDGE_GPIOD_V2
    gpiod_line_request* request_;
    unsigned int lineOffset_;
#else
    gpiod_line* line_;
#endif

    bool activeHigh_;
};
