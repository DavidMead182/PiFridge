// pifridge_api.cpp
// FastCGI endpoint for PiFridge.
// Reads /tmp/fridge_data.json and serves it to nginx on each GET /api/fridge.
//
// Build via CMake (see src/web_app/CMakeLists.txt)
// Run:
//   sudo ./build/src/web_app/pifridge_api

#include <fcgiapp.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

// Path must match the path written by saveStateToJson() in main.cpp
static const char* JSON_PATH   = "/tmp/fridge_data.json";

// Must match fastcgi_pass in config/pifridge.conf
static const char* SOCKET_PATH = "/tmp/pifridge_socket";

int main() {
    // Initialise the FastCGI library
    if (FCGX_Init() != 0) {
        std::cerr << "[pifridge_api] FCGX_Init failed\n";
        return 1;
    }

    // Open a Unix socket — nginx connects to this
    int socketFd = FCGX_OpenSocket(SOCKET_PATH, /*backlog=*/5);
    if (socketFd < 0) {
        std::cerr << "[pifridge_api] Failed to open socket: " << SOCKET_PATH << "\n";
        return 1;
    }

    FCGX_Request request;
    if (FCGX_InitRequest(&request, socketFd, 0) != 0) {
        std::cerr << "[pifridge_api] FCGX_InitRequest failed\n";
        return 1;
    }

    std::cout << "[pifridge_api] Listening on " << SOCKET_PATH << "\n";

    // ---------------------------------------------------------------------------
    // Request loop — one iteration per GET /api/fridge from the browser.
    // Follows the REST statelessness principle: each request reads the file
    // fresh and does not alter any state.
    // ---------------------------------------------------------------------------
    while (FCGX_Accept_r(&request) == 0) {

        // Read the JSON file written by saveStateToJson() in main.cpp
        std::ifstream jsonFile(JSON_PATH);
        std::string   body;

        if (jsonFile.is_open()) {
            std::ostringstream ss;
            ss << jsonFile.rdbuf();
            body = ss.str();
        } else {
            // File not yet written — pifridge may still be starting up
            body = "{\"error\": \"data not available yet\"}";
        }

        // Write HTTP headers then body
        FCGX_FPrintF(request.out,
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "\r\n"
            "%s", body.c_str());

        FCGX_Finish_r(&request);
    }

    return 0;
}