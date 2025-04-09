#include "core/user_sequence/user_sequence.h"
#include "utils/utils.h"
#include "core/postgres/queries.h"
#include "core/progress_words/progress_words.h"
#include "core/sequence_generation/sequence_generator.h"
#include "plugins/BannedWords/BannedWords.h"
#include "core/user_sequence_words/user_sequence_words.h"
#include "plugins/OpenaiGeneration/OpenaiGeneration.h"
#include "core/phrases_generation/phrases_generator.h"
#include <drogon/HttpController.h>
#include <core/user_sequence_words/user_sequence_words.h>
#include <plugins/OpenaiGeneration/OpenaiGeneration.h>
#include "core/phrases_generation/phrases_generator.h"
#include "utils/exceptions.h"
#include "core/user_sequence/user_item.h"

using namespace drogon;

namespace core::user_sequence {

Task<bool> is_user_blocked_from_generation(drogon::nosql::RedisClientPtr redis_client, uint64_t user_id) {
    const auto user_blocked_focus_group_generation_key = local_utils::redis::create_blocked_user_focus_group_generation_key(user_id);
    auto user_generation_blocked_result = co_await redis_client->execCommandCoro(
        "exists %s",
        user_blocked_focus_group_generation_key.c_str()
    );

    co_return std::stoi(user_generation_blocked_result.asString()) > 0;
}

Task<> block_user_from_generation(drogon::nosql::RedisClientPtr redis_client, uint64_t user_id) {
    const auto user_blocked_focus_group_generation_key = local_utils::redis::create_blocked_user_focus_group_generation_key(user_id);
    co_await redis_client->execCommandCoro("set %s %s ex %d nx",
        user_blocked_focus_group_generation_key.c_str(),
        "",
        local_constants::user_blocked_focus_group_generation_seconds
    );
    co_return;
}

drogon::Task<bool> is_user_got_offer(drogon::orm::DbClientPtr users_db, uint64_t user_id, OfferType offer_type) {
    auto result = co_await users_db->execSqlCoro(
        "SELECT 1 FROM user_offers WHERE user_id = $1 and offer_type = $2 LIMIT 1",
        user_id,
        static_cast<uint16_t>(offer_type)
    );
    
    co_return result.size();
}

drogon::Task<> set_user_got_offer(drogon::orm::DbClientPtr users_db, uint64_t user_id, OfferType offer_type, uint16_t user_choice) {
    auto time_now = local_utils::time::get_unix_timestamp();

    co_await users_db->execSqlCoro(
        "insert into user_offers (user_id, offer_type, user_choice, created_at) values ($1, $2, $3, $4) on conflict do nothing",
        user_id,
        static_cast<uint16_t>(offer_type),
        user_choice,
        time_now
    );

    co_return;
}

drogon::Task<bool> should_user_get_rate_us_offer(drogon::orm::DbClientPtr users_db, uint64_t user_id) {
    auto did_user_got_offer = co_await is_user_got_offer(users_db, user_id, OfferType::rate_us);

    if (!did_user_got_offer) {
        auto time_now = local_utils::time::get_unix_timestamp();
        auto result = co_await users_db->execSqlCoro(
            "select 1 from users where id = $1 and NOW() - register_date >= INTERVAL '3 hours'", user_id
        );
        co_return result.size();
    } else {
        co_return false;
    }
}

drogon::Task<> set_learn_item_passed(drogon::orm::DbClientPtr users_db, uint64_t user_id, uint32_t group_id, uint16_t item_id, 
    uint16_t mistakes, uint16_t seconds_spent, uint16_t seconds_spent_on_answer
) {
    auto time_now = local_utils::time::get_unix_timestamp();

    co_await users_db->execSqlCoro(
        " update user_items set is_passed = TRUE, passed_at = $1, mistakes_count = $2, seconds_spent = $3, seconds_spent_on_answer = $4"
        " where user_id = $5 and group_id = $6 and item_id = $7 and is_passed = FALSE",
        time_now, mistakes, seconds_spent, seconds_spent_on_answer, user_id, group_id, item_id
    );
}

drogon::Task<> set_learn_item_hidden(drogon::orm::DbClientPtr users_db, uint64_t user_id, uint32_t group_id,
    uint16_t item_id, uint16_t seconds_spent
) {
    auto time_now = local_utils::time::get_unix_timestamp();

    co_await users_db->execSqlCoro(
        " update user_items set is_hidden = TRUE, passed_at = $1, seconds_spent = $2"
        " where user_id = $3 and group_id = $4 and item_id = $5 and is_hidden = FALSE",
        time_now, seconds_spent, user_id, group_id, item_id
    );
}

drogon::Task<> set_user_item_passed(drogon::orm::DbClientPtr users_db, uint64_t user_id, uint32_t group_id, uint16_t item_id, uint16_t seconds_spent
) {
    auto time_now = local_utils::time::get_unix_timestamp();

    co_await users_db->execSqlCoro(
        " update user_items set is_passed = TRUE, passed_at = $1, seconds_spent = $2"
        " where user_id = $3 and group_id = $4 and item_id = $5",
        time_now, seconds_spent, user_id, group_id, item_id
    );
}

drogon::Task<> update_group_last_interaction_time(drogon::orm::DbClientPtr users_db, uint64_t user_id) {
    auto time_now = local_utils::time::get_unix_timestamp();

    co_await users_db->execSqlCoro(
        "update user_group set last_interaction_time = $1 where user_id = $2", time_now, user_id
    );
}

// status_code, message, did_user_hide_item
drogon::Task<std::optional<std::tuple<drogon::HttpStatusCode, std::string, bool>>> update_item_progress(
    drogon::orm::DbClientPtr users_db, uint64_t user_id, const Json::Value &item_request
) {
    co_await update_group_last_interaction_time(users_db, user_id);

    const auto& item_action = item_request["action"].as<std::string>();
    const auto& item_id_str = item_request["id"].as<std::string>();
    uint16_t seconds_spent = item_request["seconds_spent"].as<uint32_t>();

    if (!item_request.isMember("action")
        || !item_request.isMember("id")
        || !item_request.isMember("seconds_spent")
    ) {
        co_return std::make_tuple(k400BadRequest, "item_missing_fields", false);
    }

    auto [group_id, item_id, item_type] = core::user_sequence::decode_item_id(item_id_str);

    if (item_type == core::user_sequence::ItemType::first
        || item_type == core::user_sequence::ItemType::second
        || item_type == core::user_sequence::ItemType::third
    ) {
        if (!item_request.isMember("seconds_spent_on_answer")
            || !item_request.isMember("mistakes")
        ) {
            co_return std::make_tuple(k400BadRequest, "item_missing_fields", false);
        }

        uint16_t seconds_spent_on_answer = item_request["seconds_spent_on_answer"].as<uint32_t>();
        uint16_t mistakes = item_request["mistakes"].as<uint32_t>();

        if (item_action == "passed") {
            uint16_t mistakes = item_request["mistakes"].as<uint32_t>();
            co_await set_learn_item_passed(users_db, user_id, group_id, item_id, mistakes, seconds_spent, seconds_spent_on_answer);
        }
        else if (item_action == "hide") {
            co_await set_learn_item_hidden(users_db, user_id, group_id, item_id, seconds_spent);
        }
        else {
            co_return std::make_tuple(k400BadRequest, "not valid item action", false);
        }
    }
    else if (item_type == core::user_sequence::ItemType::result) {
        co_await set_user_item_passed(users_db, user_id, group_id, item_id, seconds_spent);
    }
    else if (item_type == core::user_sequence::ItemType::rate_us) {
        co_await set_user_item_passed(users_db, user_id, group_id, item_id, seconds_spent);
        if (!item_request.isMember("rated")) {
            co_return std::make_tuple(k400BadRequest, "item_missing_fields", false);
        }
        uint16_t rated = item_request["rated"].as<bool>();
        co_await core::user_sequence::set_user_got_offer(users_db, user_id, core::user_sequence::OfferType::rate_us, rated);
    }

    co_return std::nullopt;
}

uint32_t get_topic_mix_value(uint32_t topic_index, uint32_t created_at, uint32_t current_time) {
    if (topic_index == 0) return (current_time - created_at > local_constants::topics::seconds_first_topic_prioritized ? 20 : 6);
    else if (topic_index == 1) return 4;
    else if (topic_index == 2) return 3;
    else if (topic_index == 3) return 2;
    else return 1;
}

drogon::Task<std::vector<core::user_sequence_words::SequenceUserWord>> get_updated_user_sequence(drogon::orm::DbClientPtr users_db, uint64_t user_id, uint32_t sequence_last_update_id) {
    auto language_pair_topics = co_await users_db->execSqlCoro(
        " WITH max_lang_pair AS ("
            " SELECT DISTINCT ON (user_id) user_id, src_lang, dst_lang"
            " FROM user_language_pairs"
            " WHERE is_enabled = TRUE AND user_id = $1"
            " ORDER BY user_id, last_enable_time DESC"
        " )"
        " SELECT ut.src_lang, ut.dst_lang, ut.words_sequence, ut.created_at"
        " FROM user_topics ut"
        " JOIN max_lang_pair mlp"
            " ON ut.user_id = mlp.user_id"
            " AND ut.src_lang = mlp.src_lang"
            " AND ut.dst_lang = mlp.dst_lang"
        " WHERE ut.is_enabled = TRUE AND ut.user_id = $1"
        " ORDER BY ut.created_at DESC", user_id
    );

    std::string src_lang;
    std::string dst_lang;

    if (language_pair_topics.empty()) {
        co_return {};
    } else {
        src_lang = language_pair_topics[0]["src_lang"].as<std::string>();
        dst_lang = language_pair_topics[0]["dst_lang"].as<std::string>();
    }

    auto user_words_result = co_await users_db->execSqlCoro(
        " SELECT src_lang, word, is_learned, is_hidden, learn_confirmation_level, last_update_time"
        " FROM user_words WHERE user_id = $1 AND src_lang = $2", user_id, src_lang
    );

    // create sequence generator
    auto banned_words_plugin = drogon::app().getPlugin<BannedWords>();
    core::sequence_generation::SequenceGenerator sequence_generator(banned_words_plugin);

    // add user words
    auto user_words = core::progress_words::get_user_words_from_orm_sql(user_words_result);
    co_await sequence_generator.add_user_words(std::move(src_lang), std::move(user_words));

    auto current_time = local_utils::time::get_unix_timestamp();

    // add topics
    uint32_t topic_index = 0;
    for (auto& topic : language_pair_topics) {
        auto topic_src_lang = topic["src_lang"].as<std::string>();
        auto topic_dst_lang = topic["dst_lang"].as<std::string>();
        auto words_sequence = topic["words_sequence"].as<std::string>();

        auto created_at = topic["created_at"].as<uint32_t>();
        auto lang_mix_value = local_constants::topics::lang_mix_value_multiplier;
        auto topic_mix_value = static_cast<float>(get_topic_mix_value(topic_index++, created_at, current_time));

        co_await sequence_generator.add_topic_words(
            std::move(topic_src_lang),
            std::move(topic_dst_lang),
            lang_mix_value,
            topic_mix_value,
            std::move(words_sequence)
        );
    }

    std::ignore = users_db->execSqlCoro(
        " update user_group set last_gen_id = last_update_id where user_id = $1 and last_update_id = $2",
        user_id, sequence_last_update_id
    );

    co_return sequence_generator.generate();
}

drogon::Task<std::vector<core::phrases_generator::FocusGroupPhrase>> generate_group_phrases(
    drogon::orm::DbClientPtr users_db,
    uint32_t user_id,
    bool user_has_subscription,
    std::vector<core::user_sequence_words::SequenceUserWord>& user_words_sequence,
    uint32_t group_size
) {
    auto openai_phrase_generation = drogon::app().getPlugin<OpenaiGeneration>();
    auto frequent_words_plugin = drogon::app().getPlugin<FrequentWords>();
    auto translations_db = drogon::app().getDbClient(local_constants::translations_db);
    core::phrases_generator::PhrasesGenerator phrases_generator(openai_phrase_generation, frequent_words_plugin, translations_db);

    uint8_t gen_attempts_count = 0;
    constexpr uint8_t max_gen_attempts_limit = 30;

    std::vector<core::phrases_generator::FocusGroupPhrase> group_phrases;

    for (auto& user_word : user_words_sequence) {
        auto to_generate_count = group_size - group_phrases.size();

        if (to_generate_count == 0) break;

        if (++gen_attempts_count >= max_gen_attempts_limit) {
            throw exception::user_max_phrase_generation_attempts_reached();
        }

        phrases_generator.add_phrase(user_word, user_has_subscription);

        if (phrases_generator.count() == to_generate_count) {
            try {
                auto generated_phrases = co_await phrases_generator.get_generated_phrases();

                for (auto& phrase : generated_phrases) {
                    group_phrases.push_back(std::move(phrase));
                }

            } catch (const OpenaiGeneration::systems_are_busy_exception& e) {
                throw exception::systems_are_busy();
            }
        }
    }

    if (group_phrases.size() < group_size) {
        throw exception::not_enough_words_for_group_generation();
    }

    co_return group_phrases;
}

}