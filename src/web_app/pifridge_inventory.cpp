
// Handles GET and POST requests for /api/inventory
//
// GET  /api/inventory        — returns all items as JSON
// POST /api/inventory        — adds a new item (JSON body)
// POST /api/inventory/delete — deletes an item by id (JSON body)
//
// Build via CMake (see src/web_app/CMakeLists.txt)
// Run:
//   ./build/src/web_app/pifridge_inventory

#include <fcgiapp.h>
#include <sqlite3.h>
#include <iostream>
#include <sstream>
#include <string>
#include <ctime>

// Must match fastcgi_pass in config/pifridge.conf
static const char* SOCKET_PATH = "/var/run/pifridge/pifridge_inventory.sock";

// SQLite database path
static const char* DB_PATH = "/var/lib/pifridge/inventory.db";

// ---------------------------------------------------------------------------
// Database helpers
// ---------------------------------------------------------------------------

// Opens the database and creates the inventory table if it doesn't exist
sqlite3* openDb() {
    sqlite3* db = nullptr;
    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        std::cerr << "[inventory] Failed to open DB: " << sqlite3_errmsg(db) << "\n";
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
        std::cerr << "[inventory] Failed to create table: " << errMsg << "\n";
        sqlite3_free(errMsg);
        sqlite3_close(db);
        return nullptr;
    }

    return db;
}

// Returns all inventory items as a JSON array string
std::string getAllItems(sqlite3* db) {
    const char* query = "SELECT id, name, barcode, quantity, date_added FROM inventory ORDER BY date_added DESC;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, query, -1, &stmt, nullptr) != SQLITE_OK) {
        return "{\"error\": \"Failed to query inventory\"}";
    }

    std::ostringstream json;
    json << "[";
    bool first = true;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (!first) json << ",";
        first = false;

        int         id       = sqlite3_column_int(stmt, 0);
        const char* name     = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        const char* barcode  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        int         quantity = sqlite3_column_int(stmt, 3);
        const char* date     = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));

        json << "{"
             << "\"id\":"       << id                           << ","
             << "\"name\":\""   << (name    ? name    : "")     << "\","
             << "\"barcode\":\"" << (barcode ? barcode : "")    << "\","
             << "\"quantity\":" << quantity                      << ","
             << "\"date_added\":\"" << (date ? date : "")       << "\""
             << "}";
    }

    json << "]";
    sqlite3_finalize(stmt);
    return json.str();
}

// Inserts a new item — returns true on success
bool addItem(sqlite3* db, const std::string& name, const std::string& barcode, int quantity) {
    // Get current date as YYYY-MM-DD
    time_t now = time(nullptr);
    char dateBuf[11];
    strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d", localtime(&now));

    const char* query =
        "INSERT INTO inventory (name, barcode, quantity, date_added) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, query, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, name.c_str(),    -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, barcode.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int (stmt, 3, quantity);
    sqlite3_bind_text(stmt, 4, dateBuf,         -1, SQLITE_STATIC);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

// Deletes an item by id — returns true on success
bool deleteItem(sqlite3* db, int id) {
    const char* query = "DELETE FROM inventory WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, query, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, id);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool decrementItem(sqlite3* db, int id) {
    // Get current quantity
    const char* selectQuery = "SELECT quantity FROM inventory WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
 
    if (sqlite3_prepare_v2(db, selectQuery, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int(stmt, 1, id);
 
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return false;
    }
 
    int qty = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
 
    if (qty <= 1) {
        // quantity will hit 0 — delete the row entirely
        return deleteItem(db, id);
    } else {
        // decrement by 1
        const char* updateQuery = "UPDATE inventory SET quantity = quantity - 1 WHERE id = ?;";
        sqlite3_stmt* upStmt = nullptr;
        if (sqlite3_prepare_v2(db, updateQuery, -1, &upStmt, nullptr) != SQLITE_OK) return false;
        sqlite3_bind_int(upStmt, 1, id);
        bool ok = (sqlite3_step(upStmt) == SQLITE_DONE);
        sqlite3_finalize(upStmt);
        return ok;
    }
}

// Increments quantity by 1
bool incrementItem(sqlite3* db, int id) {
    const char* updateQuery = "UPDATE inventory SET quantity = quantity + 1 WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, updateQuery, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int(stmt, 1, id);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

// ---------------------------------------------------------------------------
// Simple JSON field extractor
// Pulls the value of a JSON string field: "key": "value"
// or integer field: "key": value
// ---------------------------------------------------------------------------
std::string extractJsonString(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";

    pos = json.find(':', pos);
    if (pos == std::string::npos) return "";
    pos++;

    // skip whitespace
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;

    if (json[pos] == '"') {
        pos++;
        size_t end = json.find('"', pos);
        if (end == std::string::npos) return "";
        return json.substr(pos, end - pos);
    }

    // integer value — read digits
    size_t end = pos;
    while (end < json.size() && (isdigit(json[end]) || json[end] == '-')) end++;
    return json.substr(pos, end - pos);
}

// ---------------------------------------------------------------------------
// Read the full POST body from the FastCGI request
// ---------------------------------------------------------------------------
std::string readPostBody(FCGX_Request& request) {
    const char* lenStr = FCGX_GetParam("CONTENT_LENGTH", request.envp);
    if (!lenStr) return "";

    int len = std::stoi(lenStr);
    if (len <= 0) return "";

    std::string body(static_cast<size_t>(len), '\0');
    FCGX_GetStr(&body[0], len, request.in);
    return body;
}

// ---------------------------------------------------------------------------
// Main — FastCGI request loop
// ---------------------------------------------------------------------------
int main() {
    if (FCGX_Init() != 0) {
        std::cerr << "[inventory] FCGX_Init failed\n";
        return 1;
    }

    int socketFd = FCGX_OpenSocket(SOCKET_PATH, /*backlog=*/5);
    if (socketFd < 0) {
        std::cerr << "[inventory] Failed to open socket: " << SOCKET_PATH << "\n";
        return 1;
    }

    FCGX_Request request;
    if (FCGX_InitRequest(&request, socketFd, 0) != 0) {
        std::cerr << "[inventory] FCGX_InitRequest failed\n";
        return 1;
    }

    std::cout << "[inventory] Listening on " << SOCKET_PATH << "\n";

    while (FCGX_Accept_r(&request) == 0) {

        sqlite3* db = openDb();

        const char* method  = FCGX_GetParam("REQUEST_METHOD",  request.envp);
        const char* uri     = FCGX_GetParam("REQUEST_URI",     request.envp);

        std::string responseBody;
        std::string methodStr = method ? method : "";
        std::string uriStr    = uri    ? uri    : "";

        // ------------------------------------------------------------------
        // GET /api/inventory — return all items
        // ------------------------------------------------------------------
        if (methodStr == "GET") {
            if (db) {
                responseBody = getAllItems(db);
            } else {
                responseBody = "{\"error\": \"database unavailable\"}";
            }
        }

        // POST /api/inventory/decrement — reduce quantity by 1, delete if reaches 0
        else if (methodStr == "POST" && uriStr.find("/decrement") != std::string::npos) {
            std::string body  = readPostBody(request);
            std::string idStr = extractJsonString(body, "id");
        
            if (!idStr.empty() && db) {
                bool ok = decrementItem(db, std::stoi(idStr));
                responseBody = ok ? "{\"success\": true}" : "{\"error\": \"decrement failed\"}";
            } else {
                responseBody = "{\"error\": \"missing id\"}";
            }
        }
        // POST /api/inventory/increment — increase quantity by 1
        else if (methodStr == "POST" && uriStr.find("/increment") != std::string::npos) {
            std::string body  = readPostBody(request);
            std::string idStr = extractJsonString(body, "id");
        
            if (!idStr.empty() && db) {
                bool ok = incrementItem(db, std::stoi(idStr));
                responseBody = ok ? "{\"success\": true}" : "{\"error\": \"increment failed\"}";
            } else {
                responseBody = "{\"error\": \"missing id\"}";
            }
        }

        // ------------------------------------------------------------------
        // POST /api/inventory/delete — delete an item by id
        // ------------------------------------------------------------------
        else if (methodStr == "POST" && uriStr.find("/delete") != std::string::npos) {
            std::string body = readPostBody(request);
            std::string idStr = extractJsonString(body, "id");

            if (!idStr.empty() && db) {
                bool ok = deleteItem(db, std::stoi(idStr));
                responseBody = ok ? "{\"success\": true}" : "{\"error\": \"delete failed\"}";
            } else {
                responseBody = "{\"error\": \"missing id\"}";
            }
        }

        // ------------------------------------------------------------------
        // POST /api/inventory — add a new item
        // ------------------------------------------------------------------
        else if (methodStr == "POST") {
            std::string body     = readPostBody(request);
            std::string name     = extractJsonString(body, "name");
            std::string barcode  = extractJsonString(body, "barcode");
            std::string qtyStr   = extractJsonString(body, "quantity");
            int         quantity = qtyStr.empty() ? 1 : std::stoi(qtyStr);

            if (!name.empty() && db) {
                bool ok = addItem(db, name, barcode, quantity);
                responseBody = ok ? "{\"success\": true}" : "{\"error\": \"insert failed\"}";
            } else {
                responseBody = "{\"error\": \"missing name\"}";
            }
        }

        else {
            responseBody = "{\"error\": \"method not supported\"}";
        }

        if (db) sqlite3_close(db);

        FCGX_FPrintF(request.out,
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "\r\n"
            "%s", responseBody.c_str());

        FCGX_Finish_r(&request);
    }

    return 0;
}