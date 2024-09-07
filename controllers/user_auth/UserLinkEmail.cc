#include "UserLinkEmail.h"
#include "utils/utils.h"
#include "utils/constants.h"
#include "plugins/EmailSender.h"

namespace v1
{
Task<> UserLinkEmail::getCodeForEmailLink(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    auto email = req->getHeader("email");

    if (!email.size() || !app().getPlugin<EmailSender>()->is_valid_email(email)) {
        co_return local_utils::Response::Error::ERROR(callback, k400BadRequest, "not_valid_email");
    }

    auto rc = app().getRedisClient();

    // check if not requested too much
    std::string link_code_requests = local_utils::redis::create_link_code_requests_key(email);
    try {
        auto result = co_await rc->execCommandCoro("get %s", link_code_requests.c_str());

        uint8_t requests_count = result.isNil() ? 0 : std::stoi(result.asString());

        if (requests_count >= local_constants::code_max_requests) {
            co_return local_utils::Response::Error::ERROR(callback, k429TooManyRequests, "");
        }

        co_await rc->execCommandCoro("set %s %d %s %d", 
            link_code_requests.c_str(), 
            requests_count + 1, 
            "EX", 
            local_constants::block_code_request_seconds
        );
    } catch (const std::exception &e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    // check if email already exists
    try {
        auto transaction = app().getDbClient(local_constants::users_db)->newTransaction();

        auto result = co_await transaction->execSqlCoro("select 1 from users where email = $1 limit 1", email);

        if (result.size()) {
            co_return local_utils::Response::Error::ERROR(callback, k409Conflict, "");
        }
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    // create code
    auto email_code = app().getPlugin<EmailSender>()->generate_email_code();
    try {
        co_await rc->execCommandCoro("set %s %s %s %d", 
            local_utils::redis::create_link_code_key(email).c_str(), 
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

Task<> UserLinkEmail::linkEmailToAccount(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    auto email = req->getHeader("email");
    auto code = req->getHeader("code");
    
    if (!email.size() || !app().getPlugin<EmailSender>()->is_valid_email(email) || !code.size()) {
        co_return local_utils::Response::Error::ERROR(callback, k400BadRequest, "not_valid_headers");
    }

    auto rc = app().getRedisClient();

    // check if valid attempts count
    std::string link_attempts_key = local_utils::redis::create_link_attempts_key(email);
    try {
        auto result = co_await rc->execCommandCoro("get %s", link_attempts_key.c_str());

        auto attempts_count = result.isNil() ? 0 : std::stoi(result.asString());

        if (attempts_count >= local_constants::verify_code_max_attempts) {
            co_return local_utils::Response::Error::ERROR(callback, k429TooManyRequests, "");
        }

        co_await rc->execCommandCoro("set %s %d %s %d", 
            link_attempts_key.c_str(), 
            attempts_count + 1, 
            "EX", 
            local_constants::block_code_verify_attempts_seconds
        );
    } catch (const std::exception &e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    // check if code valid
    auto link_email_code_key = local_utils::redis::create_link_code_key(email);
    try {
        auto redis_code = co_await rc->execCommandCoro("get %s", 
            link_email_code_key.c_str()
        );
        
        if (redis_code.isNil() || redis_code.asString() != code) {
            co_return local_utils::Response::Error::FORBIDDEN(callback, "not_valid_code");
        }
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    // update email
    auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);
    try {
        auto user_db = app().getDbClient(local_constants::users_db);

        auto result = co_await user_db->execSqlCoro("update users set email = $1 where id = $2", 
            email,
            user_id
        );

        if (result.affectedRows() == 0) {
            co_return local_utils::Response::Error::FORBIDDEN(callback, "not_valid_user");
        }
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    // delete verify_attempts && code && code_requests key on success
    std::string link_code_requests = local_utils::redis::create_link_code_requests_key(email);
    try {
        co_await rc->execCommandCoro("del %s", link_attempts_key.c_str());
        co_await rc->execCommandCoro("del %s", link_email_code_key.c_str());
        co_await rc->execCommandCoro("del %s", link_code_requests.c_str());
    } catch (...) {}
    
    callback(HttpResponse::newHttpResponse());
}

Task<> UserLinkEmail::linkGoogleToAccount(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
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

    // check if email already exists
    try {
        auto transaction = app().getDbClient(local_constants::users_db)->newTransaction();

        auto result = co_await transaction->execSqlCoro("select 1 from users where email = $1 limit 1", email);

        if (result.size()) {
            co_return local_utils::Response::Error::ERROR(callback, k409Conflict, "");
        }
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    // update email
    auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);
    try {
        auto user_db = app().getDbClient(local_constants::users_db);

        auto result = co_await user_db->execSqlCoro("update users set email = $1 where id = $2", 
            email,
            user_id
        );

        if (result.affectedRows() == 0) {
            co_return local_utils::Response::Error::FORBIDDEN(callback, "not_valid_user");
        }
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    callback(HttpResponse::newHttpResponse());
}
};