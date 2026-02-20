#pragma once

class IGpioOutput {
public:
    virtual ~IGpioOutput() = default;

    virtual void setHigh() = 0;
    virtual void setLow() = 0;
};
