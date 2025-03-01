#pragma once

#include <iostream>
#include <string>
#include <stdexcept>
#include <curl/curl.h>
#include "utils/utils.h"

namespace core::website_fetch {

class CurlFetchWebsite {
public:
    CurlFetchWebsite() = default;

    std::string fetch_html(const std::string& url) {
        std::string_view url_stt;
        CURL* curl;
        CURLcode res;
        std::string response_data;

        curl = curl_easy_init();
        if (!curl) {
            throw std::runtime_error("Failed to initialize cURL");
        }

        auto [scheme, host, path] = local_utils::url::extract_scheme_and_host_and_path_from_url(url);

        struct curl_slist* headers = NULL;
        std::string host_header = "Host: " + std::string(host);
        headers = curl_slist_append(headers, host_header.c_str());
        headers = curl_slist_append(headers, "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/58.0.3029.110 Safari/537.36");
        headers = curl_slist_append(headers, "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8");

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlFetchWebsite::write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            curl_easy_cleanup(curl);
            throw std::runtime_error(curl_easy_strerror(res));
        }

        curl_easy_cleanup(curl);
        return response_data;
    }

private:
    static size_t write_callback(void* contents, size_t size, size_t nmemb, void* user_data) {
        size_t total_size = size * nmemb;
        static_cast<std::string*>(user_data)->append(static_cast<char*>(contents), total_size);
        return total_size;
    }
};

}

