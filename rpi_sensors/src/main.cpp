#include "Bh1750Sensor.hpp"
#include "GpioOutput.hpp"
#include "DoorLightController.hpp"

#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/timerfd.h>
#include <unistd.h>

static void printUsage(const char* argv0) {
    std::cout
        << "Usage: " << argv0 << " [options]\n"
        << "Options:\n"
        << "  --open <lux>          Open threshold lux (default 30)\n"
        << "  --close <lux>         Close threshold lux (default 10)\n"
        << "  --interval-ms <ms>    Update interval in ms (default 200)\n"
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
    if (hasPrefix(s, "0x") || hasPrefix(s, "0X")) base = 16;
    unsigned long v = std::stoul(s, &idx, base);
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
        for (int i = 1; i < argc; i++) {
            std::string a = argv[i];

            auto needValue = [&](const std::string& name) -> std::string {
                if (i + 1 >= argc) {
                    throw std::runtime_error("missing value for " + name);
                }
                return std::string(argv[++i]);
            };

            if (a == "--open") openLux = std::stod(needValue(a));
            else if (a == "--close") closeLux = std::stod(needValue(a));
            else if (a == "--interval-ms") intervalMs = std::stoi(needValue(a));
            else if (a == "--i2c-dev") i2cDev = needValue(a);
            else if (a == "--i2c-addr") i2cAddr = parseHexByte(needValue(a));
            else if (a == "--gpio-chip") gpioChip = needValue(a);
            else if (a == "--gpio-line") gpioLine = static_cast<unsigned int>(std::stoul(needValue(a)));
            else if (a == "--active-low") activeHigh = false;
            else if (a == "--help" || a == "-h") {
                printUsage(argv[0]);
                return 0;
            }
            else {
                throw std::runtime_error("unknown option: " + a);
            }
        }

        if (closeLux > openLux) {
            throw std::runtime_error("close threshold must be <= open threshold");
        }
        if (intervalMs <= 0) {
            throw std::runtime_error("interval-ms must be > 0");
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

        int sfd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
        if (sfd < 0) {
            throw std::runtime_error("signalfd failed");
        }

        int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
        if (tfd < 0) {
            ::close(sfd);
            throw std::runtime_error("timerfd_create failed");
        }

        itimerspec its{};
        its.it_value.tv_sec = intervalMs / 1000;
        its.it_value.tv_nsec = (intervalMs % 1000) * 1000000;
        its.it_interval = its.it_value;

        if (timerfd_settime(tfd, 0, &its, nullptr) != 0) {
            ::close(tfd);
            ::close(sfd);
            throw std::runtime_error("timerfd_settime failed");
        }

        int ep = epoll_create1(EPOLL_CLOEXEC);
        if (ep < 0) {
            ::close(tfd);
            ::close(sfd);
            throw std::runtime_error("epoll_create1 failed");
        }

        epoll_event evTimer{};
        evTimer.events = EPOLLIN;
        evTimer.data.u32 = 1;
        if (epoll_ctl(ep, EPOLL_CTL_ADD, tfd, &evTimer) != 0) {
            ::close(ep);
            ::close(tfd);
            ::close(sfd);
            throw std::runtime_error("epoll_ctl add timer failed");
        }

        epoll_event evSig{};
        evSig.events = EPOLLIN;
        evSig.data.u32 = 2;
        if (epoll_ctl(ep, EPOLL_CTL_ADD, sfd, &evSig) != 0) {
            ::close(ep);
            ::close(tfd);
            ::close(sfd);
            throw std::runtime_error("epoll_ctl add signal failed");
        }

        Bh1750Sensor sensor(i2cDev, i2cAddr);
        GpioOutput gpio(gpioChip, gpioLine, activeHigh);
        DoorLightController ctl(sensor, gpio, openLux, closeLux);

        bool lastOpen = ctl.isDoorOpen();

        std::cout
            << "PiFridge light sensor demo running\n"
            << "open=" << openLux << " close=" << closeLux
            << " intervalMs=" << intervalMs
            << " i2cDev=" << i2cDev
            << " i2cAddr=0x" << std::hex << static_cast<int>(i2cAddr) << std::dec
            << " gpioChip=" << gpioChip
            << " gpioLine=" << gpioLine
            << " activeHigh=" << (activeHigh ? "true" : "false")
            << "\n";

        while (true) {
            epoll_event events[4]{};
            int n = epoll_wait(ep, events, 4, -1);
            if (n < 0) {
                if (errno == EINTR) continue;
                throw std::runtime_error("epoll_wait failed");
            }

            for (int i = 0; i < n; i++) {
                if (events[i].data.u32 == 2) {
                    signalfd_siginfo si{};
                    ::read(sfd, &si, sizeof(si));
                    std::cout << "Stopping\n";
                    ::close(ep);
                    ::close(tfd);
                    ::close(sfd);
                    return 0;
                }

                if (events[i].data.u32 == 1) {
                    std::uint64_t ticks = 0;
                    ::read(tfd, &ticks, sizeof(ticks));

                    ctl.update();

                    bool nowOpen = ctl.isDoorOpen();
                    if (nowOpen != lastOpen) {
                        std::cout
                            << "door=" << (nowOpen ? "open" : "closed")
                            << " lux=" << ctl.lastLux()
                            << "\n";
                        lastOpen = nowOpen;
                    }
                }
            }
        }
    }
    catch (const std::exception& e) {
        std::cout << "Fatal: " << e.what() << "\n";
        return 1;
    }
}
