#include "UserProgress.h"
#include "utils/utils.h"
#include "utils/constants.h"
#include "core/progress_words/progress_words.h"
#include "core/postgres/queries.h"

namespace v1
{

Task<> UserProgress::getLearnedWords(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    std::string result_body;
    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        std::multimap<uint32_t, std::string, std::greater<uint32_t>> result_words;

        {
            auto result = co_await core::sql::get_user_words(users_db, user_id);

            for (auto& row : result) {
                auto src_lang = row["src_lang"].as<std::string>();
                auto words = row["words"].as<std::string>();
                auto [d, user_words] = core::progress_words::get_user_words_from_str(words);
                words.clear();

                for (auto& user_word : user_words) {
                    if (user_word.second.is_hidden || !user_word.second.confirmation_level) {
                        continue;
                    }
                    result_words.insert({user_word.second.last_update_time, src_lang + ';'+ user_word.first});
                }
            }
        }

        for (auto& i : result_words) {
            result_body += i.second + '\n';
        }

    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, result_body);
}

Task<> UserProgress::removeLearnedWord(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
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

        auto time = local_utils::time::get_unix_timestamp();
        auto hidden_word = core::progress_words::create_learned_word_str(word, 0, time);

        co_await users_db->execSqlCoro("update user_words set words = words || $1 where src_lang = $2 and user_id = $3",
            hidden_word, src_lang, user_id
        );
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, "");
}

Task<> UserProgress::getHiddenWords(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    std::string result_body;
    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        std::multimap<uint32_t, std::string, std::greater<uint32_t>> result_words;

        {
            auto result = co_await core::sql::get_user_words(users_db, user_id);

            for (auto& row : result) {
                auto src_lang = row["src_lang"].as<std::string>();
                auto words = row["words"].as<std::string>();
                auto [d, user_words] = core::progress_words::get_user_words_from_str(words);
                words.clear();

                for (auto& user_word : user_words) {
                    if (!user_word.second.is_hidden) {
                        continue;
                    }
                    result_words.insert({user_word.second.last_update_time, src_lang + ';'+ user_word.first});
                }
            }
        }

        for (auto& i : result_words) {
            result_body += i.second + '\n';
        }

    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, result_body);
}

Task<> UserProgress::removeHiddenWord(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
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

        auto time = local_utils::time::get_unix_timestamp();
        auto hidden_word = core::progress_words::create_hidden_word_str(word, false, time);

        co_await users_db->execSqlCoro("update user_words set words = words || $1 where src_lang = $2 and user_id = $3",
            hidden_word, src_lang, user_id
        );
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, "");
}

}