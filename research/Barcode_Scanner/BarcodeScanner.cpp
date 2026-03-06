#include <iostream>
#include <functional>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <cstring>
#include <cerrno>
#include <sys/select.h>

class SerialReader {
public:
    using Callback = std::function<void(const std::string&)>;

    SerialReader(const std::string& portName, Callback cb)
        : port(portName), callback(cb), fd(-1) {}

    ~SerialReader() {
        if (fd >= 0) {
            close(fd);
        }
    }

    bool openPort() {
        fd = open(port.c_str(), O_RDONLY | O_NOCTTY);
        if (fd < 0) {
            std::cerr << "Failed to open " << port << ": " << strerror(errno) << "\n";
            return false;
        }

        termios tty{};
        if (tcgetattr(fd, &tty) != 0) {
            std::cerr << "tcgetattr failed: " << strerror(errno) << "\n";
            close(fd);
            fd = -1;
            return false;
        }

        cfsetispeed(&tty, B9600);
        cfsetospeed(&tty, B9600);

        tty.c_cflag &= ~PARENB;
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CSIZE;
        tty.c_cflag |= CS8;
        tty.c_cflag |= CREAD | CLOCAL;

        tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
        tty.c_iflag &= ~(IXON | IXOFF | IXANY);
        tty.c_oflag &= ~OPOST;

        tty.c_cc[VMIN] = 1;
        tty.c_cc[VTIME] = 0;

        if (tcsetattr(fd, TCSANOW, &tty) != 0) {
            std::cerr << "tcsetattr failed: " << strerror(errno) << "\n";
            close(fd);
            fd = -1;
            return false;
        }

        return true;
    }

    void run() {
        if (fd < 0) {
            std::cerr << "Port not open\n";
            return;
        }

        char ch;
        std::string buffer;

        while (true) {
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(fd, &readfds);

            int result = select(fd + 1, &readfds, nullptr, nullptr, nullptr);
            if (result < 0) {
                std::cerr << "select failed: " << strerror(errno) << "\n";
                break;
            }

            if (FD_ISSET(fd, &readfds)) {
                ssize_t n = read(fd, &ch, 1);
                if (n > 0) {
                    if (ch == '\n') {
                        if (!buffer.empty() && buffer.back() == '\r') {
                            buffer.pop_back();
                        }

                        callback(buffer);
                        buffer.clear();
                    } else {
                        buffer += ch;
                    }
                } else if (n < 0) {
                    std::cerr << "read failed: " << strerror(errno) << "\n";
                    break;
                }
            }
        }
    }

private:
    std::string port;
    Callback callback;
    int fd;
};

int main() {
    SerialReader reader("/dev/ttyAMA0", [](const std::string& data) {
        std::cout << "Received: " << data << "\n";
    });

    if (!reader.openPort()) {
        return 1;
    }

    std::cout << "Waiting for serial data...\n";
    reader.run();

    return 0;
}