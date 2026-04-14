// BarcodeScanner.cpp

#include "BarcodeScanner.hpp"
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <cstring>
#include <cerrno>
#include <sys/select.h>
#include <curl/curl.h>
#include <cstdint>
#include <ctime>
#include <sqlite3.h>

static const char* DB_PATH = "/var/lib/pifridge/inventory.db";

// ================== CURL Write Callback ==================
static size_t write_cb(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    std::string* s = static_cast<std::string*>(userp);
    s->append(static_cast<char*>(contents), total);
    return total;
}

// ================== Simple JSON string extractor ==================
// Pulls the value of "key": "value" from a JSON string
static std::string extractJsonString(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";

    pos = json.find(':', pos);
    if (pos == std::string::npos) return "";
    pos++;

    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;

    if (pos >= json.size() || json[pos] != '"') return "";
    pos++;

    size_t end = json.find('"', pos);
    if (end == std::string::npos) return "";

    return json.substr(pos, end - pos);
}

// ================== SQLite helpers ==================

// Opens the inventory database — same schema as pifridge_inventory.cpp
static sqlite3* openDb() {
    sqlite3* db = nullptr;
    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        std::cerr << "[BarcodeScanner] Failed to open DB: " << sqlite3_errmsg(db) << "\n";
        return nullptr;
    }

    const char* createTable =
        "CREATE TABLE IF NOT EXISTS inventory ("
        "  id         INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name       TEXT    NOT NULL,"
        "  barcode    TEXT,"
        "  quantity   INTEGER NOT NULL DEFAULT 1,"
        "  date_added TEXT    NOT NULL"
        ");";

    char* errMsg = nullptr;
    if (sqlite3_exec(db, createTable, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "[BarcodeScanner] Failed to create table: " << errMsg << "\n";
        sqlite3_free(errMsg);
        sqlite3_close(db);
        return nullptr;
    }

    return db;
}

// If the barcode already exists, increment its quantity.
// Otherwise insert a new row.
static void upsertItem(sqlite3* db, const std::string& name, const std::string& barcode) {
    // Check if barcode already exists
    const char* selectQuery = "SELECT id, quantity FROM inventory WHERE barcode = ?;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, selectQuery, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "[BarcodeScanner] Failed to prepare select\n";
        return;
    }

    sqlite3_bind_text(stmt, 1, barcode.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        // Row exists — increment quantity
        int id  = sqlite3_column_int(stmt, 0);
        int qty = sqlite3_column_int(stmt, 1);
        sqlite3_finalize(stmt);

        const char* updateQuery = "UPDATE inventory SET quantity = ? WHERE id = ?;";
        sqlite3_stmt* upStmt = nullptr;
        if (sqlite3_prepare_v2(db, updateQuery, -1, &upStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(upStmt, 1, qty + 1);
            sqlite3_bind_int(upStmt, 2, id);
            sqlite3_step(upStmt);
            sqlite3_finalize(upStmt);
            std::cout << "[BarcodeScanner] Incremented quantity for: " << name << " (id=" << id << ")\n";
        }

    } else {
        // No existing row — insert new item
        sqlite3_finalize(stmt);

        time_t now = time(nullptr);
        char dateBuf[11];
        strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d", localtime(&now));

        const char* insertQuery =
            "INSERT INTO inventory (name, barcode, quantity, date_added) VALUES (?, ?, 1, ?);";
        sqlite3_stmt* insStmt = nullptr;

        if (sqlite3_prepare_v2(db, insertQuery, -1, &insStmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(insStmt, 1, name.c_str(),    -1, SQLITE_STATIC);
            sqlite3_bind_text(insStmt, 2, barcode.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(insStmt, 3, dateBuf,         -1, SQLITE_STATIC);
            sqlite3_step(insStmt);
            sqlite3_finalize(insStmt);
            std::cout << "[BarcodeScanner] Added to inventory: " << name << " (" << barcode << ")\n";
        }
    }
}

// ================== fetch_product ==================
void fetch_product(const std::string& barcode) {
    const std::string url =
        "https://world.openfoodfacts.net/api/v2/product/" + barcode + "?fields=product_name";

    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "[BarcodeScanner] Failed to init curl\n";
        return;
    }

    std::string response;

    curl_easy_setopt(curl, CURLOPT_URL,           url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPAUTH,       CURLAUTH_BASIC);
    curl_easy_setopt(curl, CURLOPT_USERPWD,        "off:off");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        std::cerr << "[BarcodeScanner] Request failed: " << curl_easy_strerror(res) << "\n";
        return;
    }

    // Check the API status field — "status": 1 means found, 0 means not found
    std::string statusStr = extractJsonString(response, "status");
    if (statusStr == "0" || statusStr.empty()) {
        // Also check for status as integer not string — Open Food Facts returns int
        size_t statusPos = response.find("\"status\":");
        if (statusPos != std::string::npos) {
            size_t valPos = response.find(':', statusPos) + 1;
            while (valPos < response.size() && response[valPos] == ' ') valPos++;
            if (response[valPos] == '0') {
                std::cout << "[BarcodeScanner] Product not found for barcode: " << barcode << " — skipping.\n";
                return;
            }
        }
    }

    // Extract product name
    std::string productName = extractJsonString(response, "product_name");

    if (productName.empty()) {
        std::cout << "[BarcodeScanner] No product name returned for: " << barcode << " — skipping.\n";
        return;
    }

    std::cout << "[BarcodeScanner] Product found: " << productName << "\n";

    // Write to SQLite inventory
    sqlite3* db = openDb();
    if (!db) return;

    upsertItem(db, productName, barcode);
    sqlite3_close(db);
}

// ================== SerialReader Implementation ==================
BarcodeScanner::BarcodeScanner(const std::string& portName)
    : port(portName), fd(-1) {}

BarcodeScanner::~BarcodeScanner() {
    stop();
}

void BarcodeScanner::stop() {
    stopScan();
    running_ = false;
    if (wake_pipe_[1] >= 0) write(wake_pipe_[1], "x", 1);
    if (thread_.joinable()) thread_.join();
    if (fd >= 0)            { close(fd);            fd = -1;           }
    if (wake_pipe_[0] >= 0) { close(wake_pipe_[0]); wake_pipe_[0] = -1; }
    if (wake_pipe_[1] >= 0) { close(wake_pipe_[1]); wake_pipe_[1] = -1; }
}

void BarcodeScanner::registerCallback(Callback cb) {
    callback = std::move(cb);
}

void BarcodeScanner::start() {
    if (!openPort()) return;
    running_ = true;
    thread_ = std::thread(&BarcodeScanner::run, this);
}

bool BarcodeScanner::openPort() {
    fd = open(port.c_str(), O_RDWR | O_NOCTTY);
    if (fd < 0) {
        std::cerr << "Failed to open " << port << ": " << strerror(errno) << "\n";
        return false;
    }

    termios tty{};
    if (tcgetattr(fd, &tty) != 0) {
        std::cerr << "tcgetattr failed: " << strerror(errno) << "\n";
        close(fd); fd = -1;
        return false;
    }

    cfsetispeed(&tty, B9600);
    cfsetospeed(&tty, B9600);

    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |=  CS8;
    tty.c_cflag |=  CREAD | CLOCAL;

    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_oflag &= ~OPOST;

    tty.c_cc[VMIN]  = 1;
    tty.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        std::cerr << "tcsetattr failed: " << strerror(errno) << "\n";
        close(fd); fd = -1;
        return false;
    }

    if (pipe(wake_pipe_) != 0) {
        std::cerr << "pipe failed: " << strerror(errno) << "\n";
        close(fd); fd = -1;
        return false;
    }

    return true;
}

void BarcodeScanner::triggerScan() {
    if (fd < 0) return;
    tcflush(fd, TCIOFLUSH);
    const uint8_t cmd[] = { 0x7E, 0x00, 0x08, 0x01, 0x00, 0x02, 0x01, 0xAB, 0xCD };
    write(fd, cmd, sizeof(cmd));
    tcdrain(fd);
}

void BarcodeScanner::stopScan() {
    if (fd < 0) return;
    const uint8_t cmd[] = { 0x7E, 0x00, 0x08, 0x01, 0x00, 0x02, 0x00, 0xAB, 0xCD };
    write(fd, cmd, sizeof(cmd));
    tcdrain(fd);
}

bool isBarcode(const std::string& s) {
    if (s.empty()) return false;

    for (unsigned char c : s) {
        if (!std::isdigit(c)) return false;
    }
    return true;
}

void BarcodeScanner::run() {
    if (fd < 0) {
        std::cerr << "Port not open\n";
        return;
    }

    char buffer[1024];
    int minimumSizeOfBarcode = 5;
    bool barcodeEndReceived;
    std::string barcode;
    const size_t MAX_BARCODE_LEN = 32;

    while (running_) {
        barcodeEndReceived = false;
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(fd, &readfds);
        FD_SET(wake_pipe_[0], &readfds);

        int maxfd = std::max(fd, wake_pipe_[0]);

        int result = select(maxfd + 1, &readfds, nullptr, nullptr, nullptr);
        if (result < 0) {
            std::cerr << "select failed: " << strerror(errno) << "\n";
            break;
        }

        if (FD_ISSET(wake_pipe_[0], &readfds)) {
            break;
        }

        if (FD_ISSET(fd, &readfds)) {
            ssize_t n = read(fd, buffer, sizeof(buffer) - 1);

            if (n > 0) {
                std::string resp(buffer, n);
                while (!resp.empty() && (resp.back() == '\n' || resp.back() == '\r')) {
                    resp.pop_back();
                    barcodeEndReceived = true;
                }


                if (resp.size() >= (size_t)minimumSizeOfBarcode && barcodeEndReceived) {
                    callback(barcode+resp);
                    barcode.clear();
                }else if (isBarcode(resp)){ // Check if the chunk looks like part of a barcode (digits only), ignore responses from barcode scanner for sensors turning on/off and other status messages
                    if (barcode.size() + resp.size() <= MAX_BARCODE_LEN) {
                        barcode += resp;
                    } else {
                        std::cerr << "[BarcodeScanner] Barcode too long, resetting buffer\n";
                        barcode.clear();
                    }
                }
                

            } else if (n < 0) {
                std::cerr << "read failed: " << strerror(errno) << "\n";
                break;
            }
        }
    }
}