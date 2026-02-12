// Run this on raspberry Pi please
// sudo apt install -y g++ libcurl4-openssl-dev
// g++ off_fetch.cpp -o off_fetch -lcurl
//./off_fetch

#include <curl/curl.h>
#include <iostream>
#include <string>

// callback to collect response body
static size_t write_cb(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    std::string* s = static_cast<std::string*>(userp);
    s->append(static_cast<char*>(contents), total);
    return total;
}

int main() {
    const std::string url =
        "https://world.openfoodfacts.net/api/v2/product/3017624010701?fields=product_name";

    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to init curl\n";
        return 1;
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
        return 1;
    }

    curl_easy_cleanup(curl);

    std::cout << response << std::endl;

    return 0;
}