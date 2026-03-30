#ifndef BARCODE_SCANNER_HPP
#define BARCODE_SCANNER_HPP

#include <string>
#include <functional>

// Function to fetch product info from OpenFoodFacts API
void fetch_product(const std::string& number);

// Class to read data from a serial port
class SerialReader {
public:
    using Callback = std::function<void(const std::string&)>;

    SerialReader(const std::string& portName, Callback cb);
    ~SerialReader();

    bool openPort();
    void run();

private:
    std::string port;
    Callback callback;
    int fd;
};

#endif