#include "asynctask.hpp"
#include "decodehtml.hpp"
#include <curl/curl.h>
#include <fstream>
#include <sstream>

static size_t write_callback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    ((std::ostream*)userdata)->write(ptr, size * nmemb);
    return size * nmemb;
}

static std::string findConfirmUrl(const std::filesystem::path& filename) {
    std::ifstream input(filename);
    std::string line;
    while(std::getline(input, line)) {
        size_t start = line.find("/uc?export=download", 0);
        if (start != std::string::npos) {
            size_t end = line.find('"', start);
            std::string url(line, start, end - start);
            DecodeHtml(url);
            return url;
        }
    }
    return "";
}

void FetchFile::run() {
    CURL* curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_AUTOREFERER, 1L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_COOKIEJAR, "-");
    //curl_easy_setopt(curl, CURLOPT_SSL_VERIFYSTATUS, 1L);

    std::ofstream output; output.open(filename, std::ios::out | std::ios::binary);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &output);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_URL, ("https://drive.google.com/uc?export=download&id=" + fileId).c_str());

    if(curl_easy_perform(curl) == CURLE_OK) {
        char *contentType;
        curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &contentType);
        if (strncmp("text/html", contentType, 9) == 0) {
            output.close();
            curl_easy_setopt(curl, CURLOPT_URL, ("https://drive.google.com" + findConfirmUrl(filename)).c_str());
            std::filesystem::remove(filename);
            output.open(filename, std::ios::out | std::ios::binary);
            curl_easy_perform(curl);
        }
    } output.close();

    long response; curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response);
    if (response != 200) std::filesystem::remove(filename);
    curl_easy_cleanup(curl);
}

void FetchJson::run() {
    CURL* curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_AUTOREFERER, 1L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    std::stringstream buffer;
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_URL, ("https://drive.google.com/uc?export=download&id=" + fileId).c_str());

    if(curl_easy_perform(curl) == CURLE_OK) {
        try {buffer >> data;} catch (...) {}
    } curl_easy_cleanup(curl);
}

void FetchImage::run() {
    CURL* curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_AUTOREFERER, 1L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    //curl_easy_setopt(curl, CURLOPT_SSL_VERIFYSTATUS, 1L);

    filename = std::filesystem::temp_directory_path() / std::tmpnam(nullptr);
    std::ofstream output; output.open(filename, std::ios::out | std::ios::binary);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &output);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_URL, ("https://drive.google.com/uc?export=download&id=" + fileId).c_str());

    if(curl_easy_perform(curl) == CURLE_OK) {
        long response; curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response);
        if (response == 200) output.close();
    } else output.close();
    curl_easy_cleanup(curl);
}