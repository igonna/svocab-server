#include "UserLanguages.h"
#include "utils/utils.h"
#include "utils/constants.h"
#include "core/postgres/queries.h"

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
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        auto value = value_str == "true";
        {
            co_await core::sql::set_user_lang_pair_enabled_transaction(users_db, user_id, src_lang, dst_lang, value);
        }
        co_await core::sql::update_user_sequence_last_update_id(users_db, user_id);
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
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

        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);
        auto value = std::stof(value_str);

        co_await users_db->execSqlCoro("update user_language_pairs set mix_value = $4 "
            "where user_id = $1 and src_lang = $2 and dst_lang = $3", user_id, src_lang, dst_lang, value
        );
        co_await core::sql::update_user_sequence_last_update_id(users_db, user_id);
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, "");
}

Task<> UserLanguages::getLanguagePairMixOptions(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    co_return local_utils::Response::Error::ERROR(callback, k501NotImplemented, "");
}
}