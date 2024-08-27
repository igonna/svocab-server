#include "UserRegister.h"
#include "utils/utils.h"
#include "utils/constants.h"
#include <drogon/HttpController.h>
#include <drogon/orm/Exception.h>

#include <drogon/drogon.h>

using namespace drogon;

namespace v1
{
Task<> UserRegister::registerAccount(HttpRequestPtr req, std::function<void(const HttpResponsePtr&)> callback) {
    // auto topic_id = req->getHeader("topic_id");
    // auto skip_value = req->getHeader("skip_value");

    uint32_t user_id;
    uint32_t device_num;

    std::string device_refresh_token = local_utils::token::create_device_token();

    // auto ip_addr = req->peerAddr().ipNetEndian();

    // create user
    try {
        // create user
        auto transaction = co_await app().getDbClient(local_constants::users_db)->newTransactionCoro();

        auto result = co_await transaction->execSqlCoro("insert into users default values returning id, last_device_num");

        user_id = result[0]["id"].as<uint32_t>();
        device_num = result[0]["last_device_num"].as<uint32_t>();

        co_await transaction->execSqlCoro("insert into user_sequence (user_id) values ($1)", user_id);
        co_await transaction->execSqlCoro("insert into user_state (user_id) values ($1)", user_id);
        // co_await transaction->execSqlCoro("insert into user_language_pairs (user_id, src_lang, dst_lang) values ($1, $2, $3)", 
        //     user_id, lang_src, lang_dst
        // );
        // co_await transaction->execSqlCoro("insert into user_words (user_id, lang_tag) values ($1, $2)", user_id, lang_src);

        co_await transaction->execSqlCoro("insert into user_devices (user_id, device_num, device_refresh_token) values ($1, $2, $3)", 
            user_id, device_num, device_refresh_token
        );

        co_await transaction->execSqlCoro("insert into user_statistics (user_id) values ($1)", user_id);

        // if (topic_id.size() && skip_value.size()) {
        //     co_await transaction->execSqlCoro("insert into user_custom_global_topic_options (user_id, topic_id, skip_value) values ($1, $2, $3)", 
        //         user_id, std::stoi(topic_id), std::stof(skip_value)
        //     );
        // }
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Error::Response::SOMETHING_WRONG(callback);
    }

    auto resp = HttpResponse::newHttpResponse();
    resp->setBody(local_utils::token::create_device_refresh_token(user_id, device_num, device_refresh_token));
    callback(resp);
}

// Task<> UserRegister::getCodeForEmailRegister(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
//     auto email = req->getHeader("email");

//     if (!email.size() || !local_utils::email::is_valid_email(email)) {
//         co_return local_utils::Error::Response::ERROR(callback, k400BadRequest, "not_valid_email");
//     }

//     auto rc = app().getRedisClient();

//     // check if not requested too much
//     std::string register_code_requests = local_utils::redis::create_register_code_requests_key(email);
//     try {
//         auto result = co_await rc->execCommandCoro("get %s", register_code_requests.c_str());

//         uint8_t requests_count = result.isNil() ? 0 : result.asInteger();

//         if (requests_count >= local_constants::register_code_max_requests) {
//             co_return local_utils::Error::Response::ERROR(callback, k429TooManyRequests, "");
//         }

//         co_await rc->execCommandCoro("set %s %d %s %d", 
//             register_code_requests.c_str(), 
//             requests_count + 1, 
//             "EX", 
//             local_constants::register_code_max_requests
//         );
//     } catch (const std::exception &e) {
//         LOG_ERROR << e.what();
//         co_return local_utils::Error::Response::SOMETHING_WRONG(callback);
//     }

//     // check if email already exists
//     try {
//         auto transaction = app().getDbClient(local_constants::users_db)->newTransaction();

//         auto result = co_await transaction->execSqlCoro("select 1 from users where email = $1 limit 1", email);

//         if (result.size()) {
//             co_return local_utils::Error::Response::ERROR(callback, k409Conflict, "");
//         }
//     } catch (const std::exception& e) {
//         LOG_ERROR << e.what();
//         co_return local_utils::Error::Response::SOMETHING_WRONG(callback);
//     }

//     // create code
//     auto email_code = local_utils::email::generate_email_code();
//     try {
//         co_await rc->execCommandCoro("set %s %s %s %d", 
//             local_utils::redis::create_register_code_key(email).c_str(), 
//             email_code.c_str(), 
//             "EX", 
//             local_constants::register_code_expire_seconds
//         );
//     } catch (const std::exception& e) {
//         LOG_ERROR << e.what();
//         co_return local_utils::Error::Response::SOMETHING_WRONG(callback);
//     }

//     // send code to email
//     try {
//         auto& smtp_config = app().getCustomConfig()["smtp"];
//         local_utils::email::SmtpDialer(
//             smtp_config["server"].asString(), 
//             smtp_config["user"].asString(), 
//             smtp_config["password"].asString()
//         ).send_email(email, email_code);
//     } catch (const std::exception& e) {
//         LOG_ERROR << e.what();
//         co_return local_utils::Error::Response::SOMETHING_WRONG(callback);
//     }

//     callback(HttpResponse::newHttpResponse());
// }

// Task<> UserRegister::registerEmail(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
//     auto email = req->getHeader("email");

//     if (!email.size() || !local_utils::email::is_valid_email(email)) {
//         co_return local_utils::Error::Response::ERROR(callback, k400BadRequest, "not_valid_email");
//     }

//     auto rc = app().getRedisClient();

//     // check if valid attempts count
//     std::string register_attempts_key = local_utils::redis::create_register_attempts_key(email);
//     try {
//         auto result = co_await rc->execCommandCoro("get %s", register_attempts_key.c_str());

//         uint8_t attempts_count = result.isNil() ? 0 : result.asInteger();

//         if (attempts_count >= local_constants::register_max_attempts) {
//             co_return local_utils::Error::Response::ERROR(callback, k429TooManyRequests, "");
//         }
//     } catch (const std::exception &e) {
//         LOG_ERROR << e.what();
//         co_return local_utils::Error::Response::SOMETHING_WRONG(callback);
//     }

//     callback(HttpResponse::newHttpResponse());
// }

// Task<> UserRegister::registerGoogle(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
//     auto token = req->getHeader("token");

//     if (!token.size()) {
//         co_return local_utils::Error::Response::ERROR(callback, k400BadRequest, "not_valid_token");
//     }

//     std::string email;
//     try {
//         email = co_await local_utils::email::get_email_from_google_token(token);
//     } catch (const std::exception& e) {
//         LOG_ERROR << e.what();
//         co_return local_utils::Error::Response::SOMETHING_WRONG(callback);
//     }


//     Json::Value response;
//     response["email"] = email;
//     auto resp = HttpResponse::newHttpJsonResponse(response);
//     callback(resp);
// }
}