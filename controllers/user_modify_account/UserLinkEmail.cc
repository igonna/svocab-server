#include "UserLinkEmail.h"
#include "utils/utils.h"
#include "utils/constants.h"

namespace v1
{
Task<> UserLinkEmail::getCodeForEmailLink(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    auto email = req->getHeader("email");

    if (!email.size() || !local_utils::email::is_valid_email(email)) {
        co_return local_utils::Error::Response::ERROR(callback, k400BadRequest, "not_valid_email");
    }

    auto rc = app().getRedisClient();

    // check if not requested too much
    std::string link_code_requests = local_utils::redis::create_link_code_requests_key(email);
    try {
        auto result = co_await rc->execCommandCoro("get %s", link_code_requests.c_str());

        uint8_t requests_count = result.isNil() ? 0 : result.asInteger();

        if (requests_count >= local_constants::link_code_max_requests) {
            co_return local_utils::Error::Response::ERROR(callback, k429TooManyRequests, "");
        }

        co_await rc->execCommandCoro("set %s %d %s %d", 
            link_code_requests.c_str(), 
            requests_count + 1, 
            "EX", 
            local_constants::link_code_max_requests
        );
    } catch (const std::exception &e) {
        LOG_ERROR << e.what();
        co_return local_utils::Error::Response::SOMETHING_WRONG(callback);
    }

    // check if email already exists
    try {
        auto transaction = app().getDbClient(local_constants::users_db)->newTransaction();

        auto result = co_await transaction->execSqlCoro("select 1 from users where email = $1 limit 1", email);

        if (result.size()) {
            co_return local_utils::Error::Response::ERROR(callback, k409Conflict, "");
        }
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Error::Response::SOMETHING_WRONG(callback);
    }

    // create code
    auto email_code = local_utils::email::generate_email_code();
    try {
        co_await rc->execCommandCoro("set %s %s %s %d", 
            local_utils::redis::create_link_code_key(email).c_str(), 
            email_code.c_str(), 
            "EX", 
            local_constants::link_code_expire_seconds
        );
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Error::Response::SOMETHING_WRONG(callback);
    }

    // send code to email
    try {
        auto& smtp_config = app().getCustomConfig()["smtp"];
        local_utils::email::SmtpDialer(
            smtp_config["server"].asString(), 
            smtp_config["user"].asString(), 
            smtp_config["password"].asString()
        ).send_email(email, email_code);
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Error::Response::SOMETHING_WRONG(callback);
    }

    co_return callback(HttpResponse::newHttpResponse());
}

Task<> UserLinkEmail::linkEmailToAccount(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    auto email = req->getHeader("email");
    
    if (!email.size() || !local_utils::email::is_valid_email(email)) {
        co_return local_utils::Error::Response::ERROR(callback, k400BadRequest, "not_valid_email");
    }

    auto rc = app().getRedisClient();

    // check if valid attempts count
    std::string link_attempts_key = local_utils::redis::create_link_attempts_key(email);
    try {
        auto result = co_await rc->execCommandCoro("get %s", link_attempts_key.c_str());

        uint8_t attempts_count = result.isNil() ? 0 : result.asInteger();

        if (attempts_count >= local_constants::link_max_attempts) {
            co_return local_utils::Error::Response::ERROR(callback, k429TooManyRequests, "");
        }
    } catch (const std::exception &e) {
        LOG_ERROR << e.what();
        co_return local_utils::Error::Response::SOMETHING_WRONG(callback);
    }

    // link

    callback(HttpResponse::newHttpResponse());
}

Task<> UserLinkEmail::linkGoogleToAccount(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    auto token = req->getHeader("token");

    if (!token.size()) {
        co_return local_utils::Error::Response::ERROR(callback, k400BadRequest, "not_valid_token");
    }

    std::string email;
    try {
        email = co_await local_utils::email::get_email_from_google_token(token);
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Error::Response::SOMETHING_WRONG(callback);
    }


    Json::Value response;
    response["email"] = email;
    auto resp = HttpResponse::newHttpJsonResponse(response);
    callback(resp);
}
};