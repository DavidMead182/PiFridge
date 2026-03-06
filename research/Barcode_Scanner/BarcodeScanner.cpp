#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <cstring>
#include <thread>
#include <mutex>
#include <queue>
#include <chrono>

class BarcodeScanner {
public:
    BarcodeScanner(const std::string& port = "/dev/serial0", int baud = 9600) 
        : fd(-1), running(false) 
    {
        fd = open(port.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
        if (fd < 0) {
            std::cerr << "Error opening serial port: " << port << std::endl;
            return;
        }
        configurePort(baud);
    }

    ~BarcodeScanner() {
        stop();
        if (fd >= 0) close(fd);
    }

    void start() {
        if (running) return;
        running = true;
        scanThread = std::thread(&BarcodeScanner::scanLoop, this);
    }

    void stop() {
        if (!running) return;
        running = false;
        if (scanThread.joinable()) scanThread.join();
    }

    bool hasBarcode() {
        std::lock_guard<std::mutex> lock(mtx);
        return !barcodeQueue.empty();
    }

    std::string getBarcode() {
        std::lock_guard<std::mutex> lock(mtx);
        if (barcodeQueue.empty()) return "";
        std::string code = barcodeQueue.front();
        barcodeQueue.pop();
        return code;
    }

    // Send software trigger command
    void triggerScan() {
        if (fd < 0) return;
        unsigned char start_cmd = 0x16; // Most Waveshare modules
        write(fd, &start_cmd, 1);
    }

private:
    int fd;
    bool running;
    std::thread scanThread;
    std::mutex mtx;
    std::queue<std::string> barcodeQueue;

    void configurePort(int baud) {
        struct termios tty;
        memset(&tty, 0, sizeof tty);

        if (tcgetattr(fd, &tty) != 0) {
            std::cerr << "Error getting attributes\n";
            return;
        }

        cfsetospeed(&tty, B9600);
        cfsetispeed(&tty, B9600);

        tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
        tty.c_iflag = 0;
        tty.c_oflag = 0;
        tty.c_lflag = 0;
        tty.c_cc[VMIN]  = 1;
        tty.c_cc[VTIME] = 1;

        tty.c_cflag |= (CLOCAL | CREAD);
        tty.c_cflag &= ~(PARENB | PARODD);
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CRTSCTS;

        tty.c_iflag &= ~(IXON | IXOFF | IXANY);

        tcsetattr(fd, TCSANOW, &tty);
    }

    void scanLoop() {
        std::string buffer;
        char c;

        while (running) {
            int n = read(fd, &c, 1);
            if (n > 0) {
                if (c == '\n' || c == '\r') {
                    if (!buffer.empty()) {
                        std::lock_guard<std::mutex> lock(mtx);
                        barcodeQueue.push(buffer);
                        buffer.clear();
                    }
                } else {
                    buffer += c;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
};

// ------------------- Main -------------------
int main() {
    BarcodeScanner scanner("/dev/serial0", 9600);
    scanner.start();

    std::cout << "Barcode scanner running. Sending start scan..." << std::endl;

    while (true) {
        // Send the scan trigger every 1 second
        scanner.triggerScan();
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // Check for scanned barcode
        while (scanner.hasBarcode()) {
            std::string code = scanner.getBarcode();
            std::cout << "Scanned: " << code << std::endl;
        }
    }

    scanner.stop();
    return 0;
}