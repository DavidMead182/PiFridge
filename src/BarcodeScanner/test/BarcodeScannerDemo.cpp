#include "BarcodeScanner.hpp"
#include <iostream>
#include <csignal>
#include <thread>

// ---------------------------------------------------------------------------
// Signal handling - Ctrl+C shuts everything down cleanly
// ---------------------------------------------------------------------------
static std::atomic<bool> g_quit{false};
static void sigHandler(int) { g_quit = true; }

int main() {
    std::signal(SIGINT,  sigHandler);
    std::signal(SIGTERM, sigHandler);

    BarcodeScanner scanner("/dev/ttyAMA0");

    scanner.registerCallback([&](const std::string& barcode) {
        std::string code = barcode;

        std::cout << "[Barcode] Scanned: " << code << "\n";

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        scanner.triggerScan();
        
    });

    scanner.start();
    scanner.triggerScan();
    
    while (!g_quit) {
        pause();
    }

    std::cout << "\nShutting down barcode sensor...\n";
    scanner.stop();
}