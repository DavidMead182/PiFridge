#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

#include "../include/Bh1750Sensor.hpp"
#include "../include/DoorLightController.hpp"

namespace {
std::atomic<bool> g_stop{false};

void signalHandler(int) {
    g_stop = true;
}
}

int main() {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    try {
        Bh1750Sensor sensor("/dev/i2c-1", 0x23);
        DoorLightController controller(30.0, 10.0);

        sensor.registerCallback([&](double lux) {
            std::cout << "[BH1750Demo] lux=" << lux << '\n';
            controller.hasLightSample(lux);
        });

        controller.registerDoorStateCallback([](bool isOpen, double lux) {
            std::cout << "[BH1750Demo] door="
                      << (isOpen ? "open" : "closed")
                      << " lux=" << lux << '\n';
        });

        sensor.start(200);

        std::cout << "BH1750 demo running. Press Ctrl+C to stop.\n";

        while (!g_stop) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        sensor.stop();
        std::cout << "BH1750 demo stopped.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[BH1750Demo] error: " << e.what() << '\n';
        return 1;
    }
}