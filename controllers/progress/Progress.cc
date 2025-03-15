#include "Progress.h"
#include "utils/utils.h"
#include "utils/constants.h"
#include "core/progress_words/progress_words.h"
#include "core/postgres/queries.h"

namespace v1
{

Task<> Progress::getHiddenWords(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

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

        std::string current_lang;
        Json::Value result_body;

        auto& lang_hidden_words_arr = result_body["languages"] = Json::arrayValue;

        Json::Value current_words = Json::arrayValue;

        for (const auto& hidden_word : hidden_words_result) {
            const auto& src_lang = hidden_word["src_lang"].as<std::string>();
            const auto& word = hidden_word["word"].as<std::string>();

            if (current_lang != src_lang) {
                if (current_words.size()) {

                    Json::Value new_lang_hidden_words;
                    new_lang_hidden_words["src_lang"] = std::move(current_lang);
                    new_lang_hidden_words["words"] = std::move(current_words);
                    lang_hidden_words_arr.append(std::move(new_lang_hidden_words));

                    current_words.clear();
                }

                current_lang = src_lang;
            }

            current_words.append(std::move(word));
        }

        if (current_words.size()) {
            Json::Value new_lang_hidden_words;
            new_lang_hidden_words["src_lang"] = std::move(current_lang);
            new_lang_hidden_words["words"] = std::move(current_words);
            lang_hidden_words_arr.append(std::move(new_lang_hidden_words));

            current_words.clear();
        }

        co_return local_utils::Response::JSON_OK(callback, result_body);
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
            "update user_words set is_hidden = false where user_id = $1 and src_lang = $2 and word = $3",
            user_id, src_lang, word
        );

        co_return local_utils::Response::OK(callback, "");
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
    }

    co_return local_utils::Response::Error::SERVER_ERROR(callback);
}
}