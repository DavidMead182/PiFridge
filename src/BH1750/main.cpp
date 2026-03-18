#include "Bh1750Sensor.hpp"
#include "DoorLightController.hpp"
#include "GpioOutput.hpp"

#include <cerrno>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

#include <sys/signalfd.h>
#include <unistd.h>

static void printUsage(const char* argv0) {
    std::cout
        << "Usage: " << argv0 << " [options]\n"
        << "Options:\n"
        << "  --open <lux>          Open threshold lux (default 30)\n"
        << "  --close <lux>         Close threshold lux (default 10)\n"
        << "  --interval-ms <ms>    Sample interval in ms (default 200)\n"
        << "  --i2c-dev <path>      I2C device path (default /dev/i2c-1)\n"
        << "  --i2c-addr <hex>      I2C address hex (default 0x23)\n"
        << "  --gpio-chip <name>    gpiochip name (default gpiochip0)\n"
        << "  --gpio-line <n>       GPIO line offset (default 17)\n"
        << "  --active-low          Treat low as ON (default active high)\n";
}

static bool hasPrefix(const std::string& s, const std::string& p) {
    return s.size() >= p.size() && s.compare(0, p.size(), p) == 0;
}

static std::uint8_t parseHexByte(const std::string& s) {
    std::size_t idx = 0;
    int base = 10;
    if (hasPrefix(s, "0x") || hasPrefix(s, "0X")) {
        base = 16;
    }

    const unsigned long v = std::stoul(s, &idx, base);
    if (idx != s.size() || v > 0xFF) {
        throw std::runtime_error("invalid hex byte: " + s);
    }

    return static_cast<std::uint8_t>(v);
}

int main(int argc, char** argv) {
    double openLux = 30.0;
    double closeLux = 10.0;
    int intervalMs = 200;

    std::string i2cDev = "/dev/i2c-1";
    std::uint8_t i2cAddr = 0x23;

    std::string gpioChip = "gpiochip0";
    unsigned int gpioLine = 17;
    bool activeHigh = true;

    try {
        for (int i = 1; i < argc; ++i) {
            const std::string a = argv[i];

            auto needValue = [&](const std::string& name) -> std::string {
                if (i + 1 >= argc) {
                    throw std::runtime_error("missing value for " + name);
                }
                return std::string(argv[++i]);
            };

            if (a == "--open") {
                openLux = std::stod(needValue(a));
            }
            else if (a == "--close") {
                closeLux = std::stod(needValue(a));
            }
            else if (a == "--interval-ms") {
                intervalMs = std::stoi(needValue(a));
            }
            else if (a == "--i2c-dev") {
                i2cDev = needValue(a);
            }
            else if (a == "--i2c-addr") {
                i2cAddr = parseHexByte(needValue(a));
            }
            else if (a == "--gpio-chip") {
                gpioChip = needValue(a);
            }
            else if (a == "--gpio-line") {
                gpioLine = static_cast<unsigned int>(std::stoul(needValue(a)));
            }
            else if (a == "--active-low") {
                activeHigh = false;
            }
            else if (a == "--help" || a == "-h") {
                printUsage(argv[0]);
                return 0;
            }
            else {
                throw std::runtime_error("unknown option: " + a);
            }
        }

        if (intervalMs <= 0) {
            throw std::runtime_error("interval-ms must be > 0");
        }
        if (closeLux > openLux) {
            throw std::runtime_error("close threshold must be <= open threshold");
        }
    }
    catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << "\n";
        printUsage(argv[0]);
        return 2;
    }

    try {
        sigset_t mask;
        sigemptyset(&mask);
        sigaddset(&mask, SIGINT);
        sigaddset(&mask, SIGTERM);

        if (sigprocmask(SIG_BLOCK, &mask, nullptr) != 0) {
            throw std::runtime_error("sigprocmask failed");
        }

        const int sfd = signalfd(-1, &mask, SFD_CLOEXEC);
        if (sfd < 0) {
            throw std::runtime_error("signalfd failed");
        }

        Bh1750Sensor sensor(i2cDev, i2cAddr);
        GpioOutput gpio(gpioChip, gpioLine, activeHigh);
        DoorLightController controller(gpio, openLux, closeLux);

        controller.registerDoorStateCallback([](bool isOpen, double lux) {
            std::cout
                << "door=" << (isOpen ? "open" : "closed")
                << " lux=" << lux
                << "\n";
        });

        sensor.registerCallback([&controller](double lux) {
            controller.hasLightSample(lux);
        });

        sensor.start(intervalMs);

        std::cout
            << "PiFridge light sensor demo running\n"
            << "open=" << openLux
            << " close=" << closeLux
            << " intervalMs=" << intervalMs
            << " i2cDev=" << i2cDev
            << " i2cAddr=0x" << std::hex << static_cast<int>(i2cAddr) << std::dec
            << " gpioChip=" << gpioChip
            << " gpioLine=" << gpioLine
            << " activeHigh=" << (activeHigh ? "true" : "false")
            << "\n";

        while (true) {
            signalfd_siginfo si{};
            const ssize_t rc = ::read(sfd, &si, sizeof(si));

            if (rc < 0) {
                if (errno == EINTR) {
                    continue;
                }
                sensor.stop();
                ::close(sfd);
                throw std::runtime_error("read from signalfd failed");
            }

            if (rc == static_cast<ssize_t>(sizeof(si)) &&
                (si.ssi_signo == SIGINT || si.ssi_signo == SIGTERM)) {
                std::cout << "Stopping\n";
                break;
            }
        }

        sensor.stop();
        ::close(sfd);
        return 0;
    }
    catch (const std::exception& e) {
        std::cout << "Fatal: " << e.what() << "\n";
        return 1;
    }
}
