#include "BannedWords.h"
#include <drogon/HttpClient.h>
#include "utils/constants.h"
#include <drogon/drogon.h>
#include <drogon/utils/coroutine.h>
#include <cstring>
#include "core/postgres/queries.h"

using namespace drogon;

BannedWords::BannedWords() {
    for (auto lang : local_constants::languages::chat_gpt_supported_languages_sorted_by_learning_popularity) {
        auto new_banned_words_ptr = std::make_shared<BannedWordsList>();
        banned_words_by_lang.emplace(lang, new_banned_words_ptr);
    }
}

void BannedWords::initAndStart(const Json::Value &config) {
    drogon::async_run([this]() -> drogon::Task<void> {
        try {
            auto translations_db = app().getDbClient(local_constants::translations_db);
            co_await core::sql::create_translations_db_tables_for_all_langs(translations_db);

            auto task = [this]() {
                drogon::async_run([this]() -> drogon::Task<void> {
                    auto translations_db = app().getDbClient(local_constants::translations_db);

                    for (auto& banned_words : banned_words_by_lang) {
                        auto banned_words_ptr = banned_words.second.load();
                        
                        auto new_words = co_await core::sql::get_banned_words_for_src_lang_from_time_added(translations_db, banned_words.first, banned_words_ptr->last_added_time);
                        auto new_last_added_time = new_words.first;
                        auto new_banned_words = std::move(new_words.second);

                        if (new_banned_words.empty()) continue;

                        new_banned_words.insert(banned_words_ptr->words.begin(), banned_words_ptr->words.end());

                        auto new_shared_ptr = std::make_shared<BannedWordsList>(new_last_added_time, std::move(new_banned_words));
                        banned_words.second.store(new_shared_ptr);
                    }
                    co_return;
                });
            };

            task();
            app().getLoop()->runEvery(std::chrono::seconds(60), task);
        } catch (const std::exception& e) {
            LOG_ERROR << e.what();
            std::terminate();
        }
    });

    
}

void BannedWords::shutdown() {

}