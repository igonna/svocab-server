#include "website_fetch.h"
#include <regex>

namespace core::website_fetch {

    drogon::Task<std::string> fetch_website_coro(const std::string &url, int32_t redirect_limit) {
        static const std::regex url_regex(R"((https?)://([^/]+)(/.*)?)");
        std::smatch match;
        if (!std::regex_match(url, match, url_regex)) {
            throw std::invalid_argument("Invalid URL: " + url);
        }

        std::string scheme = match[1];
        std::string host = match[2];
        std::string path = match[3].matched ? match[3].str() : "/";

        auto client = drogon::HttpClient::newHttpClient(scheme + "://" + host);
        auto req = drogon::HttpRequest::newHttpRequest();
        req->setPath(path);
        req->addHeader("Host", host);
        req->addHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/58.0.3029.110 Safari/537.36");
        req->addHeader("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8");

        auto response = co_await client->sendRequestCoro(req);
    
        // Handling 3xx redirects (301, 302, 303, 307, 308)
        if ((response->getStatusCode() == drogon::k301MovedPermanently || 
            response->getStatusCode() == drogon::k302Found ||
            response->getStatusCode() == drogon::k303SeeOther ||
            response->getStatusCode() == drogon::k307TemporaryRedirect ||
            response->getStatusCode() == drogon::k308PermanentRedirect) && redirect_limit > 0) {

            auto location = response->getHeader("location");
            if (!location.empty()) {
                if (location.find("://") == std::string::npos) {
                    location = scheme + "://" + host + location;
                }
                co_return co_await fetch_website_coro(location, redirect_limit - 1);
            }
        }

        if (response->getStatusCode() == drogon::k200OK) {
            co_return std::string(response->getBody());
        }

        throw std::runtime_error("Failed to fetch website, status: " + std::to_string(response->getStatusCode()));
    }

}