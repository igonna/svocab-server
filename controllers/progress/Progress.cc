#include "Progress.h"
#include "utils/utils.h"
#include "utils/constants.h"
#include "core/progress_words/progress_words.h"
#include "core/postgres/queries.h"

namespace v1
{
Task<> Progress::getHiddenWords(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    auto to_translate_lang = req->getHeader("toTranslateLang");

    if (to_translate_lang.size() != local_constants::languages::lang_tag_size) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        auto lang_pairs_result = co_await users_db->execSqlCoro(
            " SELECT src_lang"
            " FROM user_language_pairs"
            " WHERE user_id = $1 AND is_enabled = true"
            " ORDER BY last_enable_time DESC", user_id
        );

        auto hidden_words_result = co_await users_db->execSqlCoro(
            " SELECT DISTINCT ulp.src_lang, ulp.last_enable_time, uw.word, uw.last_update_time"
            " FROM user_language_pairs AS ulp"
            " LEFT JOIN user_words AS uw"
            " ON ulp.user_id = uw.user_id "
            " AND ulp.src_lang = uw.src_lang"
            " WHERE ulp.user_id = $1"
            " AND ulp.is_enabled = true"
            " AND uw.is_hidden = true"
            " ORDER BY ulp.last_enable_time DESC, uw.last_update_time DESC", user_id
        );

        auto json_hidden_words = createHiddenWordsList(lang_pairs_result, hidden_words_result, to_translate_lang);
        co_return local_utils::Response::JSON_OK(callback, json_hidden_words);
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
    }
    co_return local_utils::Response::Error::SERVER_ERROR(callback);
}

Task<> Progress::removeHiddenWord(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    auto src_lang = req->getHeader("srcLang");
    auto word = std::string(req->getBody());

    if (src_lang.empty()) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    if (word.empty()) {
        co_return local_utils::Response::Error::NOT_VALID_BODY(callback);
    }

    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        co_await users_db->execSqlCoro(
            "update user_words set is_hidden = false, last_update_time = 0 where user_id = $1 and src_lang = $2 and word = $3",
            user_id, src_lang, word
        );

        co_await core::sql::update_user_group_last_update_id(users_db, user_id);
        co_return local_utils::Response::OK(callback, "");
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
    }

    co_return local_utils::Response::Error::SERVER_ERROR(callback);
}

Json::Value Progress::createHiddenWordsList(const drogon::orm::Result& langs_rows, const drogon::orm::Result& hidden_words_rows, const std::string& to_translate_lang) {
    Json::Value result;
    auto& languages = result["languages"] = Json::arrayValue;

    std::unordered_map<std::string, Json::Value*> languages_hidden_words;

    for (auto& row : langs_rows) {
        auto src_lang = row["src_lang"].as<std::string>();

        Json::Value new_lang_pair;
        new_lang_pair["src_lang"] = src_lang;
        new_lang_pair["src_lang_name"] = local_utils::supported_languages::get_language_title_localized(src_lang, to_translate_lang);

        auto& ref_to_hidden_words = new_lang_pair["words"] = Json::arrayValue;
        languages_hidden_words[src_lang] = &ref_to_hidden_words;

        languages.append(std::move(new_lang_pair));
    }

    for (auto& hidden_word : hidden_words_rows) {
        const auto& src_lang = hidden_word["src_lang"].as<std::string>();
        const auto& word = hidden_word["word"].as<std::string>();

        if (languages_hidden_words.contains(src_lang)) {
            auto& languages_hidden_words_ref = languages_hidden_words[src_lang];

            const auto& src_lang = hidden_word["src_lang"].as<std::string>();
            const auto& word = hidden_word["word"].as<std::string>();
            languages_hidden_words_ref->append(std::move(word));
        }
    }

    return result;
}
}