#include "Account.h"
#include "utils/utils.h"
#include "utils/constants.h"
#include <drogon/HttpController.h>
#include <drogon/orm/Exception.h>
#include <drogon/drogon.h>
#include "core/token/token.h"
#include "core/postgres/queries.h"
#include <chrono>
#include "core/device/revoke.h"
#include "plugins/EmailSender.h"

using namespace drogon;

namespace v1
{
Task<> Account::registerAccount(HttpRequestPtr req, std::function<void(const HttpResponsePtr&)> callback) {
    auto src_lang = req->getHeader("srcLang");
    auto dst_lang = req->getHeader("dstLang");

    if (src_lang.empty() || dst_lang.empty()) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    auto rc = app().getRedisClient();

    auto remote_addr = req->peerAddr().toIp();

    // check if not requested too much
    std::string register_requests_key = local_utils::redis::create_register_requests_key(remote_addr);
    try {
        auto result = co_await rc->execCommandCoro("get %s", register_requests_key.c_str());

        uint8_t requests_count = result.isNil() ? 0 : std::stoi(result.asString());

        if (requests_count >= local_constants::register_max_requests) {
            co_return local_utils::Response::Error::ERROR(callback, k429TooManyRequests, "");
        }

        co_await rc->execCommandCoro("set %s %d %s %d", 
            register_requests_key.c_str(), 
            requests_count + 1, 
            "EX", 
            local_constants::block_register_request_seconds
        );
    } catch (const std::exception &e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SERVER_ERROR(callback);
    }

    uint64_t user_id;
    uint32_t device_num;

    std::string device_refresh_token = core::token::create_device_token();

    // create user
    try {
        // create user
        auto transaction = co_await app().getDbClient(local_constants::users_db)->newTransactionCoro();

        auto result = co_await transaction->execSqlCoro("insert into users default values returning id, last_device_num");
        user_id = result[0]["id"].as<uint64_t>();
        device_num = result[0]["last_device_num"].as<uint32_t>();

        co_await transaction->execSqlCoro("insert into user_checkpoints (user_id) values ($1)", user_id);
        co_await transaction->execSqlCoro("insert into last_user_special_offer_time (user_id) values ($1)", user_id);
        co_await transaction->execSqlCoro(
            "insert into user_language_pairs (user_id, src_lang, dst_lang, is_enabled, last_enable_time)"
            " values ($1, $2, $3, $4, $5)", 
            user_id, src_lang, dst_lang, true, local_utils::time::get_unix_timestamp()
        );
        co_await transaction->execSqlCoro("insert into user_devices (user_id, device_num, device_refresh_token) values ($1, $2, $3)", 
            user_id, device_num, device_refresh_token
        );
        co_await transaction->execSqlCoro("insert into user_limits (user_id) values ($1)", user_id);
        co_await transaction->execSqlCoro("insert into user_sequence (user_id) values ($1)", user_id);
        co_await transaction->execSqlCoro("insert into user_statistics (user_id) values ($1)", user_id);

        transaction->setCommitCallback([user_id, device_num, device_refresh_token, moved_callback = std::move(callback)](bool success) -> void {
            if (success) {
                auto body = core::token::create_device_refresh_token(user_id, device_num, device_refresh_token);

                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k200OK);
                resp->setBody(body);
                moved_callback(resp);
            } else {
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k500InternalServerError);
                resp->setBody("something_wrong");
                moved_callback(resp);
            }
        });
        co_return;
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
    }
    co_return local_utils::Response::Error::SERVER_ERROR(callback);
}

Task<> Account::getAccountInfo(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    std::string email;
    bool has_subscription;

    try {
        auto users_db = app().getDbClient(local_constants::users_db);

        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        auto result = co_await users_db->execSqlCoro("SELECT u.id, u.email, CASE WHEN us.user_id IS NOT NULL THEN TRUE ELSE FALSE END "
            "AS has_subscription FROM users u LEFT JOIN user_google_play_subscription us ON u.id = us.user_id WHERE u.id = $1",
            user_id
        );

        email = result[0]["email"].as<std::string>();
        has_subscription = result[0]["has_subscription"].as<bool>();

        Json::Value json_resp;
        json_resp["email"] = email;
        json_resp["email_type"] = app().getPlugin<EmailSender>()->get_email_type(email);
        json_resp["has_subscription"] = has_subscription;

        co_return local_utils::Response::JSON_OK(callback, json_resp);
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
    }
    co_return local_utils::Response::Error::SERVER_ERROR(callback);
}

Task<> Account::deleteAccount(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    auto users_db = app().getDbClient(local_constants::users_db);
    auto refresh_token = req->getHeader("refreshToken");

    try {
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);
        auto device_num = local_utils::attributes::get_value<uint32_t>(req, local_constants::attributes::device_num);
        auto [user_id_1, device_num_1, device_refresh_token] = core::token::decode_device_refresh_token(refresh_token);

        if (user_id != user_id_1 || device_num != device_num_1) {
            throw std::runtime_error("no_match");
        }

        // check if valid device_token
        auto result = co_await users_db->execSqlCoro("select 1 from user_devices where user_id = $1 "
            "and device_num = $2 and device_refresh_token = $3 limit 1",
            user_id,
            device_num,
            device_refresh_token
        );

        if (!result.size()) {
            co_return local_utils::Response::Error::FORBIDDEN(callback, "not_valid_device_token");
        }

        // revoke all user_devices
        auto resultDevices = co_await users_db->execSqlCoro("select device_num from user_devices where user_id = $1", user_id);

        auto rc = app().getRedisClient();
        for (const auto &row : resultDevices) {
            auto cur_device_num = row["device_num"].as<uint32_t>();
            co_await core::device::revoke_device(rc, user_id, cur_device_num);
        }

        // delete account
        co_await users_db->execSqlCoro("delete from users where id = $1", user_id);
        co_return local_utils::Response::OK(callback, "");
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
    }

    co_return local_utils::Response::Error::SERVER_ERROR(callback);
}
}