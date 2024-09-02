#include "UserLanguages.h"
#include "utils/utils.h"
#include "utils/constants.h"

namespace v1
{
Task<> UserLanguages::setLanguagePairEnabled(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    auto src_lang = req->getHeader("srcLang");
    auto dst_lang = req->getHeader("dstLang");
    auto value_str = req->getHeader("value");

    if (!(src_lang.size() && dst_lang.size() && value_str.size())) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint32_t>(req, local_constants::attributes::user_id);

        co_await users_db->execSqlCoro("insert into user_language_pairs (user_id, src_lang, dst_lang, is_enabled) "
            "values ($1, $2, $3, $4) on conflict (user_id, src_lang, dst_lang) do update "
            "set is_enabled = excluded.is_enabled", user_id, src_lang, dst_lang, value_str
        );
    } catch (const std::exception& e) {
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, "");
}

Task<> UserLanguages::setLanguagePairMixValue(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    auto src_lang = req->getHeader("srcLang");
    auto dst_lang = req->getHeader("dstLang");
    auto value_str = req->getHeader("value");

    if (!(src_lang.size() && dst_lang.size() && value_str.size())) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    try {
        auto users_db = app().getDbClient(local_constants::users_db);

        auto user_id = local_utils::attributes::get_value<uint32_t>(req, local_constants::attributes::user_id);
        auto value = std::stof(value_str);

        co_await users_db->execSqlCoro("insert into user_language_pairs (user_id, src_lang, dst_lang, mix_value) "
            "values ($1, $2, $3, $4) on conflict (user_id, src_lang, dst_lang) do update "
            "set mix_value = excluded.mix_value", user_id, src_lang, dst_lang, value
        );
    } catch (const std::exception& e) {
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, "");
}
}