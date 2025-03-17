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

    co_return local_utils::Response::OK(
        callback, 
        local_utils::supported_languages::get_supported_languages_as_string(to_translate_lang)
    );
}

Task<> Languages::setLanguagePairEnabled(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    auto src_lang = req->getHeader("srcLang");
    auto dst_lang = req->getHeader("dstLang");
    auto value_str = req->getHeader("value");

    if (!(src_lang.size() && dst_lang.size() && value_str.size())) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    if (!local_utils::supported_languages::is_language_supported(src_lang) 
        || !local_utils::supported_languages::is_language_supported(dst_lang)) 
    {
        co_return local_utils::Response::Error::ERROR(callback, k400BadRequest, "language not supported");
    }

    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        auto value = value_str == "true";
        auto last_enable_time = local_utils::time::get_unix_timestamp();

        auto result = co_await users_db->execSqlCoro("insert into user_language_pairs (user_id, src_lang, dst_lang, is_enabled, last_enable_time) "
            "values ($1, $2, $3, $4, $5) on conflict (user_id, src_lang, dst_lang) do update "
            "set is_enabled = excluded.is_enabled, last_enable_time = excluded.last_enable_time", 
            user_id, src_lang, dst_lang, value, last_enable_time
        );

        if (!result.affectedRows()) {
            throw std::runtime_error("lang_pair_not_created");
        }

        co_await core::sql::update_user_sequence_last_update_id(users_db, user_id);
        co_return local_utils::Response::OK(callback, "");
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
    }
    co_return local_utils::Response::Error::SERVER_ERROR(callback);
}
}