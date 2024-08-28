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
    uint32_t user_id;
    uint32_t device_num;

    std::string device_refresh_token = local_utils::token::create_device_token();

    // create user
    try {
        // create user
        auto transaction = co_await app().getDbClient(local_constants::users_db)->newTransactionCoro();

        auto result = co_await transaction->execSqlCoro("insert into users default values returning id, last_device_num");

        user_id = result[0]["id"].as<uint32_t>();
        device_num = result[0]["last_device_num"].as<uint32_t>();

        co_await transaction->execSqlCoro("insert into user_sequence (user_id) values ($1)", user_id);
        co_await transaction->execSqlCoro("insert into user_state (user_id) values ($1)", user_id);

        co_await transaction->execSqlCoro("insert into user_devices (user_id, device_num, device_refresh_token) values ($1, $2, $3)", 
            user_id, device_num, device_refresh_token
        );

        co_await transaction->execSqlCoro("insert into user_statistics (user_id) values ($1)", user_id);

        // co_await transaction->execSqlCoro("insert into user_language_pairs (user_id, src_lang, dst_lang) values ($1, $2, $3)", 
        //     user_id, lang_src, lang_dst
        // );
        // co_await transaction->execSqlCoro("insert into user_words (user_id, lang_tag) values ($1, $2)", user_id, lang_src);

        // if (topic_id.size() && skip_value.size()) {
        //     co_await transaction->execSqlCoro("insert into user_custom_global_topic_options (user_id, topic_id, skip_value) values ($1, $2, $3)", 
        //         user_id, std::stoi(topic_id), std::stof(skip_value)
        //     );
        // }
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    auto resp = HttpResponse::newHttpResponse();
    resp->setBody(local_utils::token::create_device_refresh_token(user_id, device_num, device_refresh_token));
    co_return callback(resp);
}
}