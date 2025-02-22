#pragma once
#include <drogon/HttpClient.h>
#include <chrono>
#include "utils/constants.h"
#include <random>
#include <unordered_set>

namespace local_utils {

namespace json {
    static std::string json_to_string(const Json::Value& json) {
        Json::StreamWriterBuilder writer;
        writer["indentation"] = "";
        auto json_str = Json::writeString(writer, json);
        return json_str;
    }
}

namespace random {
    uint32_t get_random_number(uint32_t min, uint32_t max);
}

// utils
namespace attributes {
    template<typename T>
    T get_value(const drogon::HttpRequestPtr& req, const std::string& key) {
        if (!req->attributes()->find(key)) {
            throw std::runtime_error("attribute_not_exists");
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

    static uint32_t get_days_passed_since_epoch(uint32_t get_unix_timestamp) {
        constexpr uint32_t seconds_per_day = 60*60*24;
        uint32_t days_since_epoch = get_unix_timestamp / seconds_per_day;
        return days_since_epoch;
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

    static inline std::string create_register_requests_key(const std::string& ip_addr) {
        return std::string("register-ip-") + ip_addr;
    }

    static inline std::string create_focus_group_update_lock_key(uint64_t user_id) {
        return std::string("fgul-") + std::to_string(user_id);
    }

    static inline std::string create_blocked_user_focus_group_generation_key(uint64_t user_id) {
        return std::string("blcked-focus-gg-") + std::to_string(user_id);
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

    static inline void NOT_VALID_BODY(std::function<void (const drogon::HttpResponsePtr &)> &callback) {
        ERROR(callback, drogon::k400BadRequest, "not_valid_body");
    }

    static inline void SOMETHING_WRONG(std::function<void (const drogon::HttpResponsePtr &)> &callback) {
        ERROR(callback, drogon::k500InternalServerError, "something_wrong");
    }

    static inline void FORBIDDEN(std::function<void (const drogon::HttpResponsePtr &)> &callback, const std::string& body) {
        ERROR(callback, drogon::k403Forbidden, body.c_str());
    }
}
}

namespace language_pair {
    static std::string create_lang_pair_key(const std::string& src_lang, const std::string& dst_lang) {
        return src_lang + dst_lang;
    }
}

namespace supported_languages {
    std::string get_language_title_localized(const std::string& lang_tag, const std::string& to_translate_lang_tag);
    std::string get_supported_languages_as_string(const std::string& to_translate_lang_tag);
    std::string get_country_code_or_empty(const std::string& src_lang);
}

namespace strings {
    std::string get_title_from_str(const std::string& str, uint32_t length);
}

namespace translations_db {
static inline std::string create_word_phrase_table_name(const std::string& src_lang) {
    return "word_phrase_" + src_lang;
}

static inline std::string create_banned_word_table_name(const std::string& src_lang) {
    return "banned_word_" + src_lang;
}
}
}