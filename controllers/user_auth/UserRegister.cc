#include "UserRegister.h"
#include "utils/utils.h"
#include "utils/constants.h"
#include <drogon/HttpController.h>
#include <drogon/orm/Exception.h>

#include <drogon/drogon.h>
#include <chrono>

using namespace drogon;

namespace v1
{
void UserRegister::registerWithoutEmail(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) const {
    auto lang_src = req->getHeader("lang_src");
    auto lang_dst = req->getHeader("lang_dst");
    auto device_os_version = req->getHeader("device_os_version");
    auto device_model = req->getHeader("device_model");

    if (!(lang_src.size() && lang_dst.size() && device_os_version.size() && device_model.size())) {
        return local_utils::Error::Response::NOT_VALID_HEADER(callback);
    }

    auto topic_id = req->getHeader("topic_id");
    auto skip_value = req->getHeader("skip_value");

    uint32_t user_id;
    uint32_t device_num;

    std::string refresh_token = local_utils::token::generate_refresh_token();
    std::string device_name = device_os_version;

    auto ip_addr = req->peerAddr().ipNetEndian();

    // create user
    try {
        auto transaction = app().getDbClient(local_constants::users_db)->newTransaction();

        auto f = transaction->execSqlAsyncFuture("insert into users default values returning id, last_device_num");
        auto result = f.get();

        user_id = result[0]["id"].as<uint32_t>();
        device_num = result[0]["last_device_num"].as<uint32_t>();

        transaction->execSqlAsyncFuture("insert into user_sequence (user_id) values ($1)", user_id).get();
        transaction->execSqlAsyncFuture("insert into user_state (user_id) values ($1)", user_id).get();
        transaction->execSqlAsyncFuture("insert into user_language_pairs (user_id, src_lang, dst_lang) values ($1, $2, $3)", 
            user_id, lang_src, lang_dst
        ).get();
        transaction->execSqlAsyncFuture("insert into user_words (user_id, lang_tag) values ($1, $2)", user_id, lang_src).get();

        transaction->execSqlAsyncFuture("insert into user_devices (user_id, device_num, device_name, device_refresh_token, last_ip_address) values ($1, $2, $3, $4, $5)", 
            user_id, device_num, device_name, refresh_token, ip_addr
        ).get();

        transaction->execSqlAsyncFuture("insert into user_statistics (user_id) values ($1)", user_id).get();

        if (topic_id.size() && skip_value.size()) {
            transaction->execSqlAsyncFuture("insert into user_custom_global_topic_options (user_id, topic_id, skip_value) values ($1, $2, $3)", 
                user_id, std::stoi(topic_id), std::stof(skip_value)
            ).get();
        }

    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        return local_utils::Error::Response::SOMETHING_WRONG(callback);
    }

    auto resp = HttpResponse::newHttpResponse();
    resp->setBody(local_utils::token::create_client_refresh_token(user_id, device_num, refresh_token));
    callback(resp);
}

void UserRegister::getCodeForEmailRegister(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) const {
    auto email = req->getHeader("email");

    if (!email.size()) {
        return local_utils::Error::Response::ERROR(callback, k400BadRequest, "not_valid_email");
    }

    drogon::nosql::RedisClientPtr rc;

    // check if valid attempts count
    std::string register_attempts_key = local_utils::redis::create_attempts_key(email);

    try {
        rc = app().getRedisClient();

        auto result = rc->execCommandSync<uint8_t>(
            [](const drogon::nosql::RedisResult &r) {
                if (r.isNil()) {
                    return 0;
                }

                return std::stoi(r.asString());
            },
            "get %s", register_attempts_key.c_str());

        if (result >= local_constants::register_max_attempts) {
            return local_utils::Error::Response::ERROR(callback, k429TooManyRequests, "");
        }
    } catch (const std::exception &e) {
        LOG_ERROR << e.what();
        return local_utils::Error::Response::SOMETHING_WRONG(callback);
    }

    // check if email already exists
    try {
        auto transaction = app().getDbClient(local_constants::users_db)->newTransaction();

        auto f = transaction->execSqlAsyncFuture("select 1 from users where email = $1 limit 1", email);
        auto result = f.get();

        if (result.size()) {
            return local_utils::Error::Response::ERROR(callback, k409Conflict, "");
        }
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        return local_utils::Error::Response::SOMETHING_WRONG(callback);
    }


    // create code
    auto email_code = local_utils::email::generate_email_code();

    try {
        rc->execCommandSync(
            [](const drogon::nosql::RedisResult &r) {return 0;},
            "set %s %d %s %d", local_utils::redis::create_register_code_key(email).c_str(), 
            email_code.c_str(), "EX", local_constants::register_code_expire_seconds
        );
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        return local_utils::Error::Response::SOMETHING_WRONG(callback);
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
        return local_utils::Error::Response::SOMETHING_WRONG(callback);
    }

    callback(HttpResponse::newHttpResponse());
}

void UserRegister::registerEmail(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) const {
}

void UserRegister::registerGoogle(const HttpRequestPtr &req, std::function<void(const HttpResponsePtr &)> &&callback) const {

}
}