#include "UserSignIn.h"
#include "utils/utils.h"
#include "utils/constants.h"
#include "plugins/EmailSender.h"
#include "core/token/token.h"

namespace v1
{
Task<> UserSignIn::getCodeForEmailSignIn(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    auto email = req->getHeader("email");

    if (!email.size() || !app().getPlugin<EmailSender>()->is_valid_email(email)) {
        co_return local_utils::Response::Error::ERROR(callback, k400BadRequest, "not_valid_email");
    }

    auto rc = app().getRedisClient();

    // check if not requested too much
    std::string sign_in_code_requests = local_utils::redis::create_sign_in_code_requests_key(email);
    try {
        auto result = co_await rc->execCommandCoro("get %s", sign_in_code_requests.c_str());

        uint8_t requests_count = result.isNil() ? 0 : std::stoi(result.asString());

        if (requests_count >= local_constants::code_max_requests) {
            co_return local_utils::Response::Error::ERROR(callback, k429TooManyRequests, "");
        }

        co_await rc->execCommandCoro("set %s %d %s %d", 
            sign_in_code_requests.c_str(), 
            requests_count + 1, 
            "EX", 
            local_constants::block_code_request_seconds
        );
    } catch (const std::exception &e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    // check if email exists
    try {
        auto transaction = app().getDbClient(local_constants::users_db)->newTransaction();

        auto result = co_await transaction->execSqlCoro("select 1 from users where email = $1 limit 1", email);

        if (!result.size()) {
            co_return local_utils::Response::Error::ERROR(callback, k404NotFound, "email_not_exists");
        }
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    // create code
    auto email_code = app().getPlugin<EmailSender>()->generate_email_code();
    try {
        co_await rc->execCommandCoro("set %s %s %s %d", 
            local_utils::redis::create_sign_in_code_key(email).c_str(), 
            email_code.c_str(), 
            "EX", 
            local_constants::code_expire_seconds
        );
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    // send code to email
    try {
        app().getPlugin<EmailSender>()->send_email(email, email_code);
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return callback(HttpResponse::newHttpResponse());
}

Task<> UserSignIn::signInEmail(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    auto email = req->getHeader("email");
    auto code = req->getHeader("code");
    
    if (!email.size() || !app().getPlugin<EmailSender>()->is_valid_email(email) || !code.size()) {
        co_return local_utils::Response::Error::ERROR(callback, k400BadRequest, "not_valid_headers");
    }

    auto rc = app().getRedisClient();

    // check if valid attempts count
    std::string sign_in_attempts_key = local_utils::redis::create_sign_in_attempts_key(email);
    try {
        auto result = co_await rc->execCommandCoro("get %s", sign_in_attempts_key.c_str());

        auto attempts_count = result.isNil() ? 0 : std::stoi(result.asString());

        if (attempts_count >= local_constants::verify_code_max_attempts) {
            co_return local_utils::Response::Error::ERROR(callback, k429TooManyRequests, "");
        }

        co_await rc->execCommandCoro("set %s %d %s %d", 
            sign_in_attempts_key.c_str(), 
            attempts_count + 1, 
            "EX", 
            local_constants::block_code_verify_attempts_seconds
        );
    } catch (const std::exception &e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    // check if code valid
    auto sign_in_email_code_key = local_utils::redis::create_sign_in_code_key(email);
    try {
        auto redis_code = co_await rc->execCommandCoro("get %s", 
            sign_in_email_code_key.c_str()
        );
        
        if (redis_code.isNil() || redis_code.asString() != code) {
            co_return local_utils::Response::Error::FORBIDDEN(callback, "not_valid_code");
        }
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    uint64_t user_id;
    uint32_t device_num;
    std::string device_refresh_token = core::token::create_device_token();

    // update user and insert device
    try {
        // update last device num
        auto transaction = co_await app().getDbClient(local_constants::users_db)->newTransactionCoro();
        auto updateResult = co_await transaction->execSqlCoro("update users set last_device_num = last_device_num + 1 where email = $1", email);

        if (updateResult.affectedRows() == 0) {
            co_return local_utils::Response::Error::FORBIDDEN(callback, "user_not_exists");
        }

        auto selectResult = co_await transaction->execSqlCoro("select id, last_device_num from users where email = $1", email);
        
        if (selectResult.size() == 0) {
            throw std::runtime_error("failed_to_select_user_data");
        }

        user_id = selectResult[0]["id"].as<uint32_t>();
        device_num = selectResult[0]["last_device_num"].as<uint32_t>();

        co_await transaction->execSqlCoro("insert into user_devices (user_id, device_num, device_refresh_token) values ($1, $2, $3)", 
            user_id, device_num, device_refresh_token
        );
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    // delete verify_attempts && code && code_requests key on success
    std::string sign_in_code_requests = local_utils::redis::create_sign_in_code_requests_key(email);
    try {
        co_await rc->execCommandCoro("del %s", sign_in_attempts_key.c_str());
        co_await rc->execCommandCoro("del %s", sign_in_email_code_key.c_str());
        co_await rc->execCommandCoro("del %s", sign_in_code_requests.c_str());
    } catch (...) {}

    auto resp = HttpResponse::newHttpResponse();
    resp->setBody(core::token::create_device_refresh_token(user_id, device_num, device_refresh_token));
    co_return callback(resp);
}

Task<> UserSignIn::signInGoogle(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    auto token = req->getHeader("token");

    if (!token.size()) {
        co_return local_utils::Response::Error::ERROR(callback, k400BadRequest, "not_valid_token");
    }

    std::string email;
    try {
        email = co_await app().getPlugin<EmailSender>()->get_email_from_google_token(token);
    } catch (const EmailSender::google_token_not_valid_exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::FORBIDDEN(callback, e.what());
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    uint64_t user_id;
    uint32_t device_num;
    std::string device_refresh_token = core::token::create_device_token();
    
    // update user and insert device
    try {
        // update last device num
        auto transaction = co_await app().getDbClient(local_constants::users_db)->newTransactionCoro();
        auto updateResult = co_await transaction->execSqlCoro("update users set last_device_num = last_device_num + 1 where email = $1", email);

        if (updateResult.affectedRows() == 0) {
            co_return local_utils::Response::Error::FORBIDDEN(callback, "user_not_exists");
        }

        auto selectResult = co_await transaction->execSqlCoro("select id, last_device_num from users where email = $1", email);
        
        if (selectResult.size() == 0) {
            throw std::runtime_error("failed_to_select_user_data");
        }

        user_id = selectResult[0]["id"].as<uint32_t>();
        device_num = selectResult[0]["last_device_num"].as<uint32_t>();

        co_await transaction->execSqlCoro("insert into user_devices (user_id, device_num, device_refresh_token) values ($1, $2, $3)", 
            user_id, device_num, device_refresh_token
        );
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    auto resp = HttpResponse::newHttpResponse();
    resp->setBody(core::token::create_device_refresh_token(user_id, device_num, device_refresh_token));
    co_return callback(resp);
}
}