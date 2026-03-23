#include <iostream>
#include <functional>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <cstring>
#include <cerrno>
#include <sys/select.h>
#include <curl/curl.h>
#include <string>

// callback to collect response body
static size_t write_cb(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    std::string* s = static_cast<std::string*>(userp);
    s->append(static_cast<char*>(contents), total);
    return total;
}

 void fetch_product(std::string number) {
    const std::string url =
        "https://world.openfoodfacts.net/api/v2/product/" + number + "?fields=product_name";

    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to init curl\n";
        return;
    }

    std::string response;

    // === equivalent of fetch(url, { method: "GET" }) ===
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    // === equivalent of headers: { Authorization: "Basic " + btoa("off:off") } ===
    curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
    curl_easy_setopt(curl, CURLOPT_USERPWD, "off:off");
    
    // capture response body
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    // follow redirects (like fetch)
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        std::cerr << "Request failed: " << curl_easy_strerror(res) << "\n";
        curl_easy_cleanup(curl);
        return;
    }

    curl_easy_cleanup(curl);

    std::cout << response << std::endl;

    return ;
}

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
        fetch_product(data);
    });

    if (!reader.openPort()) {
        return 1;
    }

    std::cout << "Waiting for serial data...\n";
    reader.run();

    return 0;
}