#ifndef BARCODE_SCANNER_HPP
#define BARCODE_SCANNER_HPP

#include <string>
#include <functional>
#include <thread>
#include <atomic>

// Function to fetch product info from OpenFoodFacts API
void fetch_product(const std::string& number);

// Class to read data from a serial port
class BarcodeScanner {
public:
    using Callback = std::function<void(const std::string&)>;

    explicit BarcodeScanner(const std::string& portName);
    ~BarcodeScanner();

    void registerCallback(Callback cb);

    void start();   
    void stop();
    void triggerScan();
    void stopScan();

private:
    bool openPort();
    void run(); 

    std::string       port;
    Callback          callback;
    int               fd;
    int               wake_pipe_[2] = {-1, -1};
    std::thread       thread_;
    std::atomic<bool> running_{false};
};

#endif