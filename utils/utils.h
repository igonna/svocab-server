#pragma once
#include <drogon/HttpClient.h>
#include <chrono>
#include "utils/constants.h"

namespace local_utils {

// utils
namespace attributes {
    template<typename T>
    T get_value(const drogon::HttpRequestPtr& req, const std::string& key) {
        if (!req->attributes()->find(key)) {
            throw std::runtime_error("not_exists");
        }

        return req->attributes()->get<T>(key);
    }
}

// utils
namespace time {
    static uint32_t get_unix_timestamp() {
        const auto p1 = std::chrono::system_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(p1.time_since_epoch()).count();
    }
}

// utils
namespace url {
    static std::tuple<std::string, std::string> extract_host_and_path_from_url(const std::string& url) {
        std::string host;
        std::string path;

        char c = '/';
        uint32_t count = 0;
        for (char i : url) {
            if (i == c) {
                ++count;
            }

            if (count > 2) {
                path += i;
            } else {
                host += i;
            }
        }

        if (!host.size()) {
            throw std::runtime_error("not_valid_host");
        }

        return {host, path};
    }
}

// utils
namespace redis {
    static inline std::string create_link_code_requests_key(const std::string& email) {
        return std::string("link-r-") + email;
    }

    static inline std::string create_link_attempts_key(const std::string& email) {
        return std::string("link-a-") + email;
    }

    static inline std::string create_link_code_key(const std::string& email) {
        return std::string("link-c-") + email;
    }


    static inline std::string create_sign_in_code_requests_key(const std::string& email) {
        return std::string("sign_in-r-") + email;
    }

    static inline std::string create_sign_in_attempts_key(const std::string& email) {
        return std::string("sign_in-a-") + email;
    }

    static inline std::string create_sign_in_code_key(const std::string& email) {
        return std::string("sign_in-c-") + email;
    }

    static inline std::string create_supported_languages_key(const std::string& lang_tag) {
        return std::string("supported_languages-") + lang_tag;
    }
}

namespace postgres {
    static std::vector<std::string> parse_array_string(const std::string& array_str) {
        std::vector<std::string> result;

        auto str_size = array_str.size();

        std::string current_item;
        char prev_char = 0;

        bool started = false;
        char stop_symbol = 0;

        for (uint32_t i = 0; i < str_size; ++i) {
            if (started && array_str[i] == stop_symbol || stop_symbol == ',' && array_str[i] == '}') {
                if (stop_symbol == '"' && prev_char == '\\') {
                    continue;
                }

                result.push_back(current_item);
                current_item = "";
                started = false;
                stop_symbol = 0;

            } else if (!stop_symbol) {
                if (!started && array_str[i] == ',') continue;
                if (array_str[i] == '{' || array_str[i] == '}') continue;
                stop_symbol = (array_str[i] == '"') ? '"' : ',';
                started = true;
                if (stop_symbol == '"') continue;
            }

            if (started) {
                current_item += array_str[i];
                prev_char = array_str[i];
            }
        }
        return result;
    }
}

// core
namespace Response {
    static inline void OK(std::function<void (const drogon::HttpResponsePtr &)> &callback, const std::string& body) {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k200OK);
        resp->setBody(body);
        callback(resp);
    }

    static inline void JSON_OK(std::function<void (const drogon::HttpResponsePtr &)> &callback, const Json::Value& body) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    }
namespace Error {
    static inline void ERROR(std::function<void (const drogon::HttpResponsePtr &)> &callback, drogon::HttpStatusCode status_code, const char* message) {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(status_code);
        resp->setBody(message);
        callback(resp);
    }

    static inline void NOT_VALID_HEADER(std::function<void (const drogon::HttpResponsePtr &)> &callback) {
        ERROR(callback, drogon::k400BadRequest, "not_valid_headers");
    }

    static inline void SOMETHING_WRONG(std::function<void (const drogon::HttpResponsePtr &)> &callback) {
        ERROR(callback, drogon::k500InternalServerError, "something_wrong");
    }

    static inline void FORBIDDEN(std::function<void (const drogon::HttpResponsePtr &)> &callback, const std::string& body) {
        ERROR(callback, drogon::k403Forbidden, body.c_str());
    }
}
}

namespace supported_languages{
    std::string get_language_title_localized(const std::string& lang_tag, const std::string& to_translate_lang_tag);
    std::string get_supported_languages_as_string(const std::string& to_translate_lang_tag);
}
}