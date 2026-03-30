#include "BarcodeScanner.hpp"
#include <iostream>

int main() {
    SerialReader reader("/dev/ttyAMA0", [](const std::string& data) {
        std::cout << "Received: " << data << "\n";
        fetch_product(data);
    });

    if (!reader.openPort()) {
        return 1;
    }

    std::cout << "Waiting for serial data...\n";
    reader.run();

    return 0;
}