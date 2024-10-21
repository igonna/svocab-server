#include "UserRegister.h"
#include "utils/utils.h"
#include "utils/constants.h"
#include <drogon/HttpController.h>
#include <drogon/orm/Exception.h>
#include <drogon/drogon.h>
#include "core/token/token.h"
#include "core/postgres/queries.h"

using namespace drogon;

namespace v1
{
Task<> UserRegister::registerAccount(HttpRequestPtr req, std::function<void(const HttpResponsePtr&)> callback) {
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
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    uint64_t user_id;
    uint32_t device_num;

    std::string device_refresh_token = core::token::create_device_token();

    // create user
    try {
        // create user
        {
            auto transaction = co_await app().getDbClient(local_constants::users_db)->newTransactionCoro();

            auto result = co_await transaction->execSqlCoro("insert into users default values returning id, last_device_num");

            user_id = result[0]["id"].as<uint64_t>();
            device_num = result[0]["last_device_num"].as<uint32_t>();

            co_await transaction->execSqlCoro("insert into user_sequence (user_id) values ($1)", user_id);
            co_await transaction->execSqlCoro("insert into user_phrase_generation_limit (user_id) values ($1)", user_id);

            co_await transaction->execSqlCoro("insert into user_devices (user_id, device_num, device_refresh_token) values ($1, $2, $3)", 
                user_id, device_num, device_refresh_token
            );

            co_await transaction->execSqlCoro("insert into user_statistics (user_id) values ($1)", user_id);
            co_await transaction->execSqlCoro("insert into user_checkpoints (user_id) values ($1)", user_id);
        }

    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }
    
    // enable first language pair
    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto ress = co_await users_db->execSqlCoro("select id, last_device_num from users where id = $1", user_id);
        LOG_ERROR << "user_id: " << ress[0]["id"].as<uint32_t>() << " last_device_num: " << ress[0]["last_device_num"].as<uint32_t>();
        {
            co_await core::sql::set_user_lang_pair_enabled_transaction(users_db, user_id, src_lang, dst_lang, true);
        }
        co_await core::sql::update_user_sequence_last_update_id(users_db, user_id);
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    auto resp = HttpResponse::newHttpResponse();
    resp->setBody(core::token::create_device_refresh_token(user_id, device_num, device_refresh_token));
    co_return callback(resp);
}
}