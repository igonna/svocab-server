#include "EmailSender.h"
#include <random>
#include <regex>
#include <unicode/unistr.h>
#include <unicode/normalizer2.h>
#include <drogon/HttpClient.h>
#include <curl/curl.h>

using namespace drogon;

void EmailSender::initAndStart(const Json::Value &config)
{
    smtp_server = config["server"].asString();
    smtp_user = config["user"].asString();
    smtp_password = config["password"].asString();
}

void EmailSender::shutdown() 
{
}

std::string EmailSender::generate_email_code() {
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


bool EmailSender::is_valid_email(const std::string &email) {
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

Task<std::string> EmailSender::get_email_from_google_token(const std::string &token) {
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

size_t EmailSender::readfunction(char *ptr, size_t size, size_t nmemb) {
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

void EmailSender::send_email(const std::string& email, const std::string& code) {
    if (!(smtp_user.size() && smtp_password.size() && smtp_server.size())) {
        throw std::runtime_error("not valid smtp config");
    }

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
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, EmailSender::static_readfunction);

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

std::string EmailSender::get_email_type(const std::string& email) {
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