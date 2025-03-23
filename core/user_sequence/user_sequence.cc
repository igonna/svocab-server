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

namespace core::user_sequence {

drogon::Task<SequenceInfo> get_sequence_info(drogon::orm::DbClientPtr users_db, uint64_t user_id) {
    const uint32_t seconds_duration_to_repeat = local_constants::confirmation_level_1_repeat_timeout_seconds;
    auto current_time = local_utils::time::get_unix_timestamp();

    const uint32_t seconds_duration_focus_group_expire = local_constants::focus_group::focus_group_expire_seconds;

    auto user_sequence_result = co_await users_db->execSqlCoro(
        " SELECT"
            " (last_gen_id != last_update_id OR $1 - last_gen_time > $2) AS gen_required,"
            " focus_group_id,"
            " last_update_id,"
            " focus_group_size,"
                " CASE"
                    " WHEN (last_gen_id != last_update_id OR $1 - last_gen_time > $2) THEN ''"
                    " ELSE to_learn_words"
                " END AS to_learn_words,"
                " CASE"
                    " WHEN ($1 - focus_group_last_update_time > $3) OR (COALESCE((focus_group->>'ci')::int, 0) < focus_group_size) AND (last_gen_id != last_update_id OR $1 - last_gen_time > $2) OR focus_group = '{}'::jsonb THEN NULL"
                    " ELSE focus_group"
                " END AS focus_group"
        " FROM user_sequence WHERE user_id = $4",
        current_time, seconds_duration_to_repeat, seconds_duration_focus_group_expire, user_id
    );

    co_return SequenceInfo(
        user_sequence_result[0]["gen_required"].as<bool>(),
        user_sequence_result[0]["last_update_id"].as<uint32_t>(),
        std::move(user_sequence_result[0]["to_learn_words"].as<std::string>()),
        user_sequence_result[0]["focus_group"].isNull(),
        user_sequence_result[0]["focus_group_id"].as<uint32_t>(),
        user_sequence_result[0]["focus_group_size"].as<uint32_t>(),
        std::move(user_sequence_result[0]["focus_group"].as<Json::Value>())
    );
}

uint32_t get_topic_mix_value(uint32_t topic_index, uint32_t created_at, uint32_t current_time) {
    if (topic_index == 0) return (current_time - created_at > local_constants::topics::seconds_first_topic_prioritized ? 20 : 6);
    else if (topic_index == 1) return 4;
    else if (topic_index == 2) return 3;
    else if (topic_index == 3) return 2;
    else return 1;
}

drogon::Task<std::vector<core::user_sequence_words::SequenceUserWord>> get_updated_user_sequence(drogon::orm::DbClientPtr users_db, uint64_t user_id, bool is_sequence_generation_required, uint32_t sequence_last_update_id, const std::string& user_words_str) {
    if (!is_sequence_generation_required) {
        auto user_words_sequence = core::user_sequence_words::sequence_user_words_to_vector(user_words_str);
        co_return user_words_sequence;
    }

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

    auto generated_sequence = sequence_generator.generate();
    auto learning_sequence_str = sequence_generator.get_learning_sequence_str(generated_sequence);
    current_time = local_utils::time::get_unix_timestamp();

    co_await users_db->execSqlCoro(
        " update user_sequence set last_gen_id = last_update_id, last_gen_time = $1, to_learn_words = $2, focus_group = '{}'"
        " where last_update_id = $3 and user_id = $4",
        current_time, learning_sequence_str, sequence_last_update_id, user_id
    );

    co_return generated_sequence;
}

drogon::Task<Json::Value> get_updated_focus_group(drogon::orm::DbClientPtr users_db, uint64_t user_id, bool is_focus_group_generation_required, std::vector<core::user_sequence_words::SequenceUserWord>& sequence_words) {
    co_return Json::nullValue;
}

}