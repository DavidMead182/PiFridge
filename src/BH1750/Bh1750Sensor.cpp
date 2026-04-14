#include "Bh1750Sensor.hpp"

#include <chrono>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <utility>

#include <fcntl.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include <linux/i2c-dev.h>

namespace {
// BH1750 startup and measurement mode commands.
constexpr std::uint8_t kCmdPowerOn = 0x01;
constexpr std::uint8_t kCmdReset = 0x07;
constexpr std::uint8_t kCmdContinuousHighRes = 0x10;
constexpr double kLuxDivisor = 1.2;
}

Bh1750Sensor::Bh1750Sensor(std::string i2cDevicePath, std::uint8_t i2cAddress)
    : fd_(-1),
      devPath_(std::move(i2cDevicePath)),
      addr_(i2cAddress),
      callback_(),
      running_(false),
      stopFd_(-1) {
    fd_ = ::open(devPath_.c_str(), O_RDWR);
    if (fd_ < 0) {
        throw std::runtime_error("Bh1750Sensor open failed: " + devPath_);
    }

    if (::ioctl(fd_, I2C_SLAVE, addr_) < 0) {
        ::close(fd_);
        fd_ = -1;
        throw std::runtime_error("Bh1750Sensor ioctl I2C_SLAVE failed");
    }

    // Power up the device and switch it into continuous high-resolution mode.
    writeCommand(kCmdPowerOn);
    writeCommand(kCmdReset);
    writeCommand(kCmdContinuousHighRes);

    // eventfd is used to wake the blocked worker thread during shutdown.
    stopFd_ = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (stopFd_ < 0) {
        ::close(fd_);
        fd_ = -1;
        throw std::runtime_error("Bh1750Sensor eventfd failed");
    }
}

Bh1750Sensor::~Bh1750Sensor() {
    stop();

    if (stopFd_ >= 0) {
        ::close(stopFd_);
        stopFd_ = -1;
    }

    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

void Bh1750Sensor::registerCallback(LightLevelCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    callback_ = std::move(callback);
}

void Bh1750Sensor::start(int intervalMs) {
    if (intervalMs <= 0) {
        throw std::invalid_argument("intervalMs must be > 0");
    }
    if (running_) {
        throw std::logic_error("Bh1750Sensor already running");
    }

    // Clear any stale stop signal before starting a new worker thread.
    std::uint64_t drained = 0;
    while (::read(stopFd_, &drained, sizeof(drained)) == static_cast<ssize_t>(sizeof(drained))) {
    }

    running_ = true;
    worker_ = std::thread(&Bh1750Sensor::runLoop, this, intervalMs);
}

void Bh1750Sensor::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    // Wake the worker so it can observe running_ and exit cleanly.
    const std::uint64_t one = 1;
    (void)::write(stopFd_, &one, sizeof(one));

    if (worker_.joinable()) {
        worker_.join();
    }
}

void Bh1750Sensor::writeCommand(std::uint8_t cmd) {
    const int rc = ::write(fd_, &cmd, 1);
    if (rc != 1) {
        throw std::runtime_error("Bh1750Sensor write command failed");
    }
}

double Bh1750Sensor::readLuxOnce() {
    std::uint8_t buf[2] = {0, 0};
    const int rc = ::read(fd_, buf, 2);
    if (rc != 2) {
        throw std::runtime_error("Bh1750Sensor read failed");
    }

    const std::uint16_t raw =
        static_cast<std::uint16_t>((static_cast<std::uint16_t>(buf[0]) << 8) | buf[1]);

    return static_cast<double>(raw) / kLuxDivisor;
}

void Bh1750Sensor::runLoop(int intervalMs) {
    const int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
    if (tfd < 0) {
        std::cerr << "Bh1750Sensor timerfd_create failed\n";
        running_ = false;
        return;
    }

    itimerspec its{};
    its.it_value.tv_sec = intervalMs / 1000;
    its.it_value.tv_nsec = (intervalMs % 1000) * 1000000;
    its.it_interval = its.it_value;

    if (timerfd_settime(tfd, 0, &its, nullptr) != 0) {
        std::cerr << "Bh1750Sensor timerfd_settime failed\n";
        ::close(tfd);
        running_ = false;
        return;
    }

    pollfd fds[2]{};
    fds[0].fd = tfd;
    fds[0].events = POLLIN;
    fds[1].fd = stopFd_;
    fds[1].events = POLLIN;

    bool havePreviousDispatch = false;
    std::chrono::steady_clock::time_point previousDispatchStart{};

    while (running_) {
        const int rc = ::poll(fds, 2, -1);
        if (rc < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::cerr << "Bh1750Sensor poll failed: " << std::strerror(errno) << "\n";
            break;
        }

        if (fds[1].revents & POLLIN) {
            std::uint64_t stopValue = 0;
            (void)::read(stopFd_, &stopValue, sizeof(stopValue));
            break;
        }

        if (fds[0].revents & POLLIN) {
            std::uint64_t expirations = 0;
            if (::read(tfd, &expirations, sizeof(expirations)) != static_cast<ssize_t>(sizeof(expirations))) {
                continue;
            }

            try {
                const auto dispatchStart = std::chrono::steady_clock::now();
                const double lux = readLuxOnce();
                const auto afterRead = std::chrono::steady_clock::now();

                // Copy the callback under the mutex, then invoke it outside the lock.
                LightLevelCallback callbackCopy;
                {
                    std::lock_guard<std::mutex> lock(callbackMutex_);
                    callbackCopy = callback_;
                }

                if (callbackCopy) {
                    callbackCopy(lux);
                }

                const auto dispatchEnd = std::chrono::steady_clock::now();

                const auto readUs =
                    std::chrono::duration_cast<std::chrono::microseconds>(afterRead - dispatchStart).count();
                const auto callbackUs =
                    std::chrono::duration_cast<std::chrono::microseconds>(dispatchEnd - afterRead).count();
                const auto totalUs =
                    std::chrono::duration_cast<std::chrono::microseconds>(dispatchEnd - dispatchStart).count();

                long long intervalUs = -1;
                if (havePreviousDispatch) {
                    intervalUs = std::chrono::duration_cast<std::chrono::microseconds>(
                                     dispatchStart - previousDispatchStart)
                                     .count();
                }

                std::cerr << "BH1750_METRIC"
                          << " interval_us=" << intervalUs
                          << " read_us=" << readUs
                          << " callback_us=" << callbackUs
                          << " total_us=" << totalUs
                          << " lux=" << lux
                          << "\n";

                previousDispatchStart = dispatchStart;
                havePreviousDispatch = true;
            }
            catch (const std::exception& e) {
                std::cerr << "Bh1750Sensor sample error: " << e.what() << "\n";
            }
        }
    }

    ::close(tfd);
    running_ = false;
}