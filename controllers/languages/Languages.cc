#include "Languages.h"
#include "utils/utils.h"
#include "utils/constants.h"
#include "core/postgres/queries.h"
#include "core/sequence_generation/sequence_generator.h"

namespace v1
{
Task<> Languages::getSupportedLanguages(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    auto to_translate_lang = req->getHeader("toTranslateLang");

    if (to_translate_lang.size() != local_constants::languages::lang_tag_size) {
        local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    auto supported_languages_key = local_utils::redis::create_supported_languages_key(to_translate_lang);
    auto rc = app().getRedisClient();

    try {
        auto result = co_await rc->execCommandCoro("get %s", supported_languages_key.c_str());

        if (!result.isNil()) {
            co_return local_utils::Response::OK(callback, result.asString());
        }
    } catch (const std::exception &e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    std::string supported_languages = local_utils::supported_languages::get_supported_languages_as_string(to_translate_lang);

    try {
        co_await rc->execCommandCoro("set %s %s %s %d", 
            supported_languages_key.c_str(), 
            supported_languages.c_str(),
            "EX",
            local_constants::languages_str_expire_sconds
        );

    } catch (const std::exception &e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, supported_languages);
}

Task<> Languages::setLanguagePairEnabled(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    auto src_lang = req->getHeader("srcLang");
    auto dst_lang = req->getHeader("dstLang");
    auto value_str = req->getHeader("value");

    if (!(src_lang.size() && dst_lang.size() && value_str.size())) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        auto value = value_str == "true";

        auto last_enable_time = local_utils::time::get_unix_timestamp();
        
        {
            auto transaction = co_await users_db->newTransactionCoro();

            co_await transaction->execSqlCoro("insert into user_language_pairs (user_id, src_lang, dst_lang, is_enabled, last_enable_time) "
                "values ($1, $2, $3, $4, $5) on conflict (user_id, src_lang, dst_lang) do update "
                "set is_enabled = excluded.is_enabled, last_enable_time = excluded.last_enable_time", 
                user_id, src_lang, dst_lang, value, last_enable_time
            );

            co_await transaction->execSqlCoro("insert into user_words (user_id, src_lang) values ($1, $2)"
                " on conflict (user_id, src_lang) do nothing", user_id, src_lang
            );
        }

        co_await core::sql::update_user_sequence_last_update_id(users_db, user_id);
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, "");
}
}