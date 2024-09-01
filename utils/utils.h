#pragma once
#include <drogon/HttpRequest.h>
#include <random>
#include <tuple>
#include <curl/curl.h>
#include <unicode/unistr.h>
#include <unicode/normalizer2.h>
#include <regex>
#include <iostream>
#include <drogon/HttpClient.h>
#include <jwt-cpp/jwt.h>
#include <chrono>
#include "utils/constants.h"
#include <drogon/nosql/RedisClient.h>


namespace local_utils {

namespace attributes {
    template<typename T>
    T get_value(const HttpRequestPtr& req, const std::string& key) {
        if (!req->attributes()->find(key)) {
            throw std::runtime_error("not_exists");
        }

        return req->attributes()->get<T>(key);
    }
}

namespace time {
    static uint32_t get_unix_timestamp() {
        const auto p1 = std::chrono::system_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(p1.time_since_epoch()).count();
    }
}

namespace token {
    static inline std::string generate_random_string(size_t length) {
        static const std::string characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

        std::random_device rd;
        std::mt19937 generator(rd());
        std::uniform_int_distribution<> distribution(0, characters.size() - 1);

        std::string result;
        for (size_t i = 0; i < length; ++i) {
            result += characters[distribution(generator)];
        }
    
        return result; 
    }

    static uint32_t secret_part_len = 6;

    static std::string create_device_refresh_token(uint32_t user_id, uint32_t device_num, const std::string& device_refresh_token) {
        auto secret_1 = generate_random_string(secret_part_len);
        auto secret_2 = generate_random_string(secret_part_len);

        auto secret = secret_1 + secret_2;

        auto token = jwt::create()
            .set_type("JWS")
            .set_payload_claim("user_id", jwt::claim(std::to_string(user_id)))
            .set_payload_claim("device_num", jwt::claim(std::to_string(device_num)))
            .set_payload_claim("device_token", jwt::claim(device_refresh_token))
            .sign(jwt::algorithm::hs256{secret});

        return secret_1 + token + secret_2;
    }

    static inline std::string create_device_token() {
        return generate_random_string(local_constants::device_token_length);
    }

    // user_id, device_num, device_refresh_token
    static std::tuple<uint32_t, uint32_t, std::string> decode_device_refresh_token(const std::string& refresh_token) {
        uint32_t refresh_token_length = refresh_token.size();

        auto secret = refresh_token.substr(0, secret_part_len) 
            + refresh_token.substr(refresh_token_length - secret_part_len, secret_part_len);
        auto token = refresh_token.substr(secret_part_len, refresh_token_length - secret_part_len - secret_part_len);

        auto decoded = jwt::decode(token);
        auto verifier = jwt::verify().allow_algorithm(jwt::algorithm::hs256{secret});
        verifier.verify(decoded);

        auto user_id = std::stoi(decoded.get_payload_claim("user_id").as_string());
        auto device_num = std::stoi(decoded.get_payload_claim("device_num").as_string());
        auto device_token = decoded.get_payload_claim("device_token").as_string();

        return std::tuple<uint32_t, uint32_t, std::string>(user_id, device_num, device_token);
    }

    static std::string create_device_bare_token(uint32_t user_id, uint32_t device_num, const std::string& secret) {
        auto token = jwt::create()
            .set_type("JWS")
            .set_payload_claim("user_id", jwt::claim(std::to_string(user_id)))
            .set_payload_claim("device_num", jwt::claim(std::to_string(device_num)))
            .set_expires_at(std::chrono::system_clock::now() + std::chrono::seconds{local_constants::secret_key_expire_seconds})
            .sign(jwt::algorithm::hs256{secret});

        return token;
    }

    // user_id, device_id
    static std::tuple<uint32_t, uint32_t> decode_device_bare_token(const std::string& token, const std::string& secret) {
        auto decoded = jwt::decode(token);
        auto verifier = jwt::verify().allow_algorithm(jwt::algorithm::hs256{secret});
        verifier.verify(decoded);

        // Check if the token has expired
        auto exp_claim = decoded.get_payload_claim("exp");
        std::time_t exp_time = exp_claim.as_date().time_since_epoch().count();
        std::time_t time_now = ::local_utils::time::get_unix_timestamp();

        if (time_now >= exp_time) {
            throw std::runtime_error("token_expired");
        }

        auto user_id = std::stoi(decoded.get_payload_claim("user_id").as_string());
        auto device_num = std::stoi(decoded.get_payload_claim("device_num").as_string());

        return std::tuple<uint32_t, uint32_t>(user_id, device_num);
    }
}

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

namespace revoke {
    static inline std::string create_revoke_device_key(uint32_t user_id, uint32_t device_num) {
        return std::string("revoke-") + std::to_string(user_id) + "-" + std::to_string(device_num);
    }

    static Task<> revoke_device(drogon::nosql::RedisClientPtr rc, uint32_t user_id, uint32_t device_num) {
        auto revoke_key = create_revoke_device_key(user_id, device_num);

        auto result = co_await rc->execCommandCoro("set %s 0 %s %d",
            revoke_key.c_str(),
            "EX",
            local_constants::secret_key_expire_seconds
        );
        co_return;
    }

    static Task<bool> is_device_revoked(drogon::nosql::RedisClientPtr rc, uint32_t user_id, uint32_t device_num) {
        auto revoke_key = create_revoke_device_key(user_id, device_num);
        auto result = co_await rc->execCommandCoro("exists %s", revoke_key.c_str());

        if (std::stoi(result.asString()) > 0) {
            co_return true;
        }
        co_return false;
    }
}
}

namespace email {
    static inline std::string generate_email_code() {
        static constexpr uint8_t length = 5;

        std::random_device rd; 
        std::mt19937 gen(rd()); 
        std::uniform_int_distribution<> dis(48, 57); 
    
        std::string result; 
        result.reserve(length); 
    
        for (uint8_t i = 0; i < length; ++i) { 
            result += static_cast<char>(dis(gen)); 
        } 
    
        return result; 
    }


    static bool is_valid_email(const std::string &email) {
        UErrorCode error_code = U_ZERO_ERROR;
        const icu::Normalizer2* normalizer = icu::Normalizer2::getNFKCInstance(error_code);
        if (U_FAILURE(error_code)) {
            return false;
        }
        icu::UnicodeString normalized_email = normalizer->normalize(icu::UnicodeString::fromUTF8(email), error_code);
        if (U_FAILURE(error_code)) {
            return false;
        }

        std::string normalized_email_utf8;
        normalized_email.toUTF8String(normalized_email_utf8);

        std::regex email_pattern(R"((^[^\s@]+@[^\s@]+\.[^\s@]+$))", std::regex::icase);

        if (!std::regex_match(normalized_email_utf8, email_pattern)) {
            return false;
        }

        return true;
    }

    struct google_token_not_valid_exception : public std::exception{
        const char* what() const noexcept override {
            return "invalid_token";
        }
    };

    static Task<std::string> get_email_from_google_token(const std::string &token) {
        auto client = HttpClient::newHttpClient("https://oauth2.googleapis.com");
        auto req = HttpRequest::newHttpRequest();
        req->setPath("/tokeninfo?id_token=" + token);

        auto resp = co_await client->sendRequestCoro(req);

        Json::Value root;
        if (!Json::Reader().parse(std::string(resp->getBody()), root)) {
            throw std::runtime_error("json_parse_error");
        }

        if (resp->getStatusCode() != k200OK) {
            throw google_token_not_valid_exception();
            // throw std::runtime_error("not_valid_token: " + std::to_string(resp->getStatusCode()) + " " + root["error"].asString());
        }

        if (root.isMember("error")) {
            throw std::runtime_error("error_in_google_response");
        }

        auto email = root["email"].asString();

        if (!is_valid_email(email)) {
            throw std::runtime_error("not_valid_email");
        }
        co_return email;
    }

    class SmtpDialer {
    private:
        std::string smtp_server;
        std::string smtp_user;
        std::string smtp_password;

        std::string payload;

        size_t bytes_read = 0;

        size_t readfunction(char *ptr, size_t size, size_t nmemb) {
            const char *data;
            size_t room = size * nmemb;
            
            if((size == 0) || (nmemb == 0) || ((size*nmemb) < 1)) {
                return 0;
            }
            
            data = payload.data() + bytes_read;
            
            if (data) {
                size_t len = strlen(data);
                if(room < len)
                    len = room;
                memcpy(ptr, data, len);
                bytes_read += len;
            
                return len;
            }
            
            return 0;
        }


        static size_t static_readfunction(void *buffer, size_t size, size_t nmemb, void *user_data) {
            return static_cast<SmtpDialer*>(user_data)->readfunction(reinterpret_cast<char*>(buffer), size, nmemb);
        }


    public:
        SmtpDialer(const std::string &server, const std::string& user, const std::string& password) {
            smtp_server = server;
            smtp_user = user;
            smtp_password = password;
        }

        void send_email(const std::string& email, const std::string& code) {
            payload = "To: " + email + "\r\n" +
                "From: svocab <" + smtp_user + ">\r\n" +
                "Subject: code " + code + "\r\n" +
                "Content-Type: text/html\r\n" +
                "\r\n" +
                "<html><body><h1>code: " + code + "</h1></body></html>\r\n";

            CURL *curl;
            CURLcode res = CURLE_OK;
            struct curl_slist *recipients = nullptr;

            curl = curl_easy_init();
            if (curl) {
                // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
                curl_easy_setopt(curl, CURLOPT_USERNAME, smtp_user.c_str());
                curl_easy_setopt(curl, CURLOPT_PASSWORD, smtp_password.c_str());
                curl_easy_setopt(curl, CURLOPT_URL, smtp_server.c_str());
                curl_easy_setopt(curl, CURLOPT_MAIL_FROM, smtp_user.c_str());

                recipients = curl_slist_append(recipients, email.c_str());
                curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
                curl_easy_setopt(curl, CURLOPT_READFUNCTION, SmtpDialer::static_readfunction);

                curl_easy_setopt(curl, CURLOPT_READDATA, this);
                curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

                res = curl_easy_perform(curl);

                if (res != CURLE_OK) {
                    throw std::runtime_error(curl_easy_strerror(res));
                }

                curl_slist_free_all(recipients);
                curl_easy_cleanup(curl);
            }
        }
    };

    static std::string get_email_type(const std::string& email) {
        const uint32_t len = email.size();

        if (!len) return "";

        std::string type;

        uint32_t start_pos = len;
        for (uint32_t i = 0; i < len; ++i) {
            if (email[i] == '@') {
                start_pos = i;
            }

            if (i > start_pos) {
                if (email[i] == '.') break;
                type += email[i];
            }
        }

        if (type == "gmail") return "google";
        return "";
    } 
}


namespace Response {
    static inline void OK(std::function<void (const HttpResponsePtr &)> &callback, const std::string& body) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k200OK);
        resp->setBody(body);
        callback(resp);
    }
namespace Error {
namespace Name {
    static constexpr char not_valid_header[] = "not_valid_headers";
    static constexpr char SOMETHING_WRONG[] = "something_wrong";
}

    static inline void ERROR(std::function<void (const HttpResponsePtr &)> &callback, drogon::HttpStatusCode status_code, const char* message) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(status_code);
        resp->setBody(message);
        callback(resp);
    }

    static inline void NOT_VALID_HEADER(std::function<void (const HttpResponsePtr &)> &callback) {
        ERROR(callback, k400BadRequest, Name::not_valid_header);
    }

    static inline void SOMETHING_WRONG(std::function<void (const HttpResponsePtr &)> &callback) {
        ERROR(callback, k500InternalServerError, Name::SOMETHING_WRONG);
    }

    static inline void FORBIDDEN(std::function<void (const HttpResponsePtr &)> &callback, const std::string& body) {
        ERROR(callback, k403Forbidden, body.c_str());
    }
}
}

namespace secret_key {
    static Task<std::string> get_secret_key(drogon::nosql::RedisClientPtr rc) {
        auto secret_value = co_await rc->execCommandCoro("get %s", local_constants::redis::secret_key_name.c_str());

        if (secret_value.isNil()) {
            // if failed to get (not exists)
            auto new_secret_value = local_utils::token::generate_random_string(local_constants::redis::secret_key_length);
            // try to set
            auto new_secret_set_result = co_await rc->execCommandCoro("set %s %s ex %d nx", 
                local_constants::redis::secret_key_name.c_str(), 
                new_secret_value.c_str(), 
                local_constants::secret_key_expire_seconds
            );

            if (new_secret_set_result.isNil()) {
                // if failed to set (another server just set it)
                secret_value = co_await rc->execCommandCoro("get %s", local_constants::redis::secret_key_name.c_str());
            } else {
                // if set by this server
                co_return new_secret_value;
            }
        }
        co_return secret_value.asString();
    }
}
}