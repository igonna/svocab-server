#include <drogon/orm/DbClient.h>
#include <drogon/utils/coroutine.h>
#include <string>
#include <vector>
#include "core/topic_generation/topic_generator.h"
#include "core/text_processing/text_processor.h"
#include "utils/utils.h"

namespace core::sql {

static drogon::Task<> update_user_sequence_last_update_id(drogon::orm::DbClientPtr users_db, uint64_t user_id) {
    auto result = co_await users_db->execSqlCoro("update user_sequence set last_update_id = last_update_id + 1 where user_id = $1", user_id);
    if (!result.affectedRows()) {
        throw std::runtime_error("user_sequence_not_updated");
    }
    co_return;
}

static drogon::Task<std::string> get_user_topic_src_lang(drogon::orm::DbClientPtr users_db, uint64_t user_id, uint64_t topic_id) {
    auto result = co_await users_db->execSqlCoro("select src_lang from user_topics where user_id = $1 and topic_id = $2", user_id, topic_id);
    if (!result.size()) {
        throw std::runtime_error("topic_not_exists");
    }
    co_return result[0]["src_lang"].as<std::string>();
}

static drogon::Task<uint32_t> get_topic_new_source_id(drogon::orm::DbClientPtr users_db, uint64_t user_id, uint64_t topic_id) {
    auto result = co_await users_db->execSqlCoro("update user_topics "
        "set last_source_id = last_source_id + 1 where user_id = $1 and topic_id = $2 returning last_source_id", 
        user_id, topic_id
    );

    if (!result.affectedRows()) {
        throw std::runtime_error("topic_not_exists");
    }

    co_return result[0]["last_source_id"].as<uint32_t>();
}

static drogon::Task<uint32_t> add_topic_source(drogon::orm::DbClientPtr users_db, uint64_t user_id, uint64_t topic_id, const std::string& title, 
    const std::string& type, const std::string& processed_words, const std::string& text = "", const std::string& fail_type = "") 
{
    auto new_topic_source_id = co_await get_topic_new_source_id(users_db, user_id, topic_id);
    auto new_path = std::string("{") + std::to_string(new_topic_source_id) + "}";

    Json::Value metadata;
    metadata["title"] = title;
    metadata["type"] = type;
    if (fail_type.size()) {
        metadata["fail_type"] = fail_type;
    }

    auto result = co_await users_db->execSqlCoro(
        "update user_topics set "
        "sources_metadata = jsonb_set(sources_metadata, $3, $4), "
        "sources_unprocessed = jsonb_set(sources_unprocessed, $3, to_jsonb($5::text)), "
        "sources_processed = jsonb_set(sources_processed, $3, to_jsonb($6::text)), "
        "last_update_id = last_update_id + 1 "
        "where user_id = $1 and topic_id = $2"
        , user_id, topic_id, new_path, metadata.toStyledString(), text, processed_words
    );

    if (!result.affectedRows()) {
        throw std::runtime_error("can_not_update");
    }

    co_return new_topic_source_id;
}

static drogon::Task<> update_topic_source(drogon::orm::DbClientPtr users_db, uint64_t user_id, uint64_t topic_id, uint32_t source_id, const std::string& title, 
    const std::string& type, const std::string& processed_words, const std::string& text = "", const std::string& fail_type = "") 
{
    auto path = std::string("{") + std::to_string(source_id) + "}";

    Json::Value metadata;
    metadata["title"] = title;
    metadata["type"] = type;
    if (fail_type.size()) {
        metadata["fail_type"] = fail_type;
    }

    auto result = co_await users_db->execSqlCoro(
        "update user_topics set "
        "sources_metadata = jsonb_set(sources_metadata, $3, $4), "
        "sources_unprocessed = jsonb_set(sources_unprocessed, $3, to_jsonb($5::text)), "
        "sources_processed = jsonb_set(sources_processed, $3, to_jsonb($6::text)), "
        "last_update_id = last_update_id + 1 "
        "where user_id = $1 and topic_id = $2"
        , user_id, topic_id, path, metadata.toStyledString(), text, processed_words
    );

    if (!result.affectedRows()) {
        throw std::runtime_error("can_not_update");
    }
}

// generate topic sequence and update 
static drogon::Task<std::string> generate_topic_sequence_and_if_update(drogon::orm::DbClientPtr users_db, uint64_t user_id, uint64_t topic_id, uint32_t topic_last_update_id) {
    std::string result_sequence;

    // fetch topic gen data
    auto topic_sources = co_await users_db->execSqlCoro(
        "select src_lang, skip_value, order_words_by, "
        "exclude_word_if_not_present_in_all_sources, exclude_word_if_very_rare, "
        "key, value from user_topics, jsonb_each_text(sources_processed) "
        "where user_id = $1 and topic_id = $2",
        user_id, topic_id
    );

    if (topic_sources.empty()) {
        co_return result_sequence;
    }

    auto src_lang = topic_sources[0]["src_lang"].as<std::string>();
    auto skip_value = topic_sources[0]["skip_value"].as<float>();
    auto order_words_by = topic_sources[0]["order_words_by"].as<uint16_t>();
    auto exclude_word_if_not_present_in_all_sources = topic_sources[0]["exclude_word_if_not_present_in_all_sources"].as<bool>();
    auto exclude_word_if_very_rare = topic_sources[0]["exclude_word_if_very_rare"].as<bool>();

    auto topic_generator = core::topic_generation::TopicGenerator(
        src_lang,
        skip_value,
        static_cast<core::topic_generation::TopicGenerator::OrderWordsBy>(order_words_by),
        exclude_word_if_not_present_in_all_sources,
        exclude_word_if_very_rare
    );

    for (auto& source : topic_sources) {
        auto source_processed_words = source["value"].as<std::string>();
        auto words = core::text_processing::processed_words_str_to_vector(source_processed_words);
        topic_generator.add_topic_words(words);
    }

    auto new_sequence = topic_generator.get_topic_sequence_as_string();

    try {
        auto topic_sequence_update_result = co_await users_db->execSqlCoro(
            "update user_topics set last_gen_id = $1, topic_words_sequence = $2 "
            "where user_id = $3 and topic_id = $4 and last_update_id = $1", 
            topic_last_update_id, new_sequence, user_id, topic_id
        );
    } catch (const std::exception& e) {
    }

    co_return new_sequence;
}

static drogon::Task<bool> is_user_of_sepcial_user_type(drogon::orm::DbClientPtr users_db, uint64_t user_id, std::string special_user_type) {
    auto result = co_await users_db->execSqlCoro("select 1 from users where id = $1 and "
        "special_user_type = '" + special_user_type + "'::SpecialUserType limit 1", user_id
    );

    co_return result.size();
}

static drogon::Task<std::string> get_global_topic_words_sequence(drogon::orm::DbClientPtr users_db, uint32_t topic_id) {
    auto result = co_await users_db->execSqlCoro("select words from global_topics where topic_id = $1", topic_id);
    co_return result[0]["words"].as<std::string>();
}

static drogon::Task<std::string> get_topic_words_sequence_and_if_update(drogon::orm::DbClientPtr users_db, uint64_t user_id, uint64_t topic_id) {
    // check if generation required
    auto result_update_required = co_await users_db->execSqlCoro(
        "select last_update_id from user_topics "
        "where user_id = $1 and topic_id = $2 and last_gen_id != last_update_id limit 1", 
        user_id, topic_id
    );

    std::string topic_words_sequence;
    if (result_update_required.size()) {
        auto topic_last_update_id = result_update_required[0]["last_update_id"].as<uint32_t>();
        topic_words_sequence = co_await core::sql::generate_topic_sequence_and_if_update(users_db, user_id, topic_id, topic_last_update_id);
    } else {
        auto result = co_await users_db->execSqlCoro("select topic_words_sequence from user_topics "
            "where user_id = $1 and topic_id = $2", user_id, topic_id
        );

        if (!result.size()) {
            throw std::runtime_error("not_exists");
        }

        topic_words_sequence = result[0]["topic_words_sequence"].as<std::string>();
    }

    co_return topic_words_sequence;
}

static drogon::Task<int32_t> get_new_smallest_topic_position(drogon::orm::DbClientPtr users_db, uint64_t user_id, const std::string& src_lang, const std::string& dst_lang) {
    auto result_position = co_await users_db->execSqlCoro("SELECT COALESCE(MIN(position), 0) AS smallest_position "
        "FROM (SELECT position FROM user_topics WHERE user_id = $1 AND src_lang = $2 AND dst_lang = $3 AND is_enabled = TRUE "
        "UNION ALL "
        "SELECT position FROM user_global_topics "
        "WHERE user_id = $1 AND src_lang = $2 AND dst_lang = $3 AND is_enabled = TRUE) AS combined_positions", user_id, src_lang, dst_lang
    );

    co_return result_position[0]["smallest_position"].as<int32_t>() - 1;
}

static drogon::Task<int32_t> get_new_biggest_topic_position(drogon::orm::DbClientPtr users_db, uint64_t user_id, const std::string& src_lang, const std::string& dst_lang) {
    auto result_position = co_await users_db->execSqlCoro("SELECT COALESCE(MAX(position), 0) AS biggest_position "
        "FROM (SELECT position FROM user_topics WHERE user_id = $1 AND src_lang = $2 AND dst_lang = $3 AND is_enabled = TRUE "
        "UNION ALL "
        "SELECT position FROM user_global_topics "
        "WHERE user_id = $1 AND src_lang = $2 AND dst_lang = $3 AND is_enabled = TRUE) AS combined_positions", user_id, src_lang, dst_lang
    );

    co_return result_position[0]["biggest_position"].as<int32_t>() + 1;
}

static drogon::Task<> update_all_user_topics_sequences(drogon::orm::DbClientPtr users_db, uint64_t user_id) {
    // update not updated topics
    auto topics_to_generate = co_await users_db->execSqlCoro(
        "select topic_id, last_update_id from user_topics "
        "where user_id = $1 and last_gen_id != last_update_id and is_enabled = true", 
        user_id
    );
    
    for (const auto& i : topics_to_generate) {
        auto topic_id = i["topic_id"].as<uint64_t>();
        auto topic_last_update_id = i["last_update_id"].as<uint32_t>();
        co_await core::sql::generate_topic_sequence_and_if_update(users_db, user_id, topic_id, topic_last_update_id);
    }
}

static drogon::Task<drogon::orm::Result> get_user_words(drogon::orm::DbClientPtr users_db, uint64_t user_id) {
    auto result = co_await users_db->execSqlCoro("select distinct ulp.src_lang, uw.words, last_update_id from user_language_pairs as ulp"
        " left join user_words as uw"
        " on ulp.user_id = uw.user_id and ulp.src_lang = uw.src_lang"
        " where ulp.user_id = $1 and ulp.is_enabled = true", user_id
    );
    co_return result;
}

static drogon::Task<drogon::orm::Result> get_user_topics_sequences(drogon::orm::DbClientPtr users_db, uint64_t user_id) {
    auto result = co_await users_db->execSqlCoro(" ("
        " select"
            " ulp.src_lang,"
            " ulp.dst_lang,"
            " ulp.mix_value as lang_mix_value,"
            " ut.mix_value,"
            " ut.position,"
            " ulp.last_enable_time as ulp_last_enable_time,"
            " ut.topic_words_sequence as words_sequence"
            " from user_language_pairs as ulp"
            " left join user_topics as ut"
            " on ut.user_id = ulp.user_id"
            " and ut.src_lang = ulp.src_lang"
            " and ut.dst_lang = ulp.dst_lang"
            " where ulp.user_id = $1"
            " and ulp.is_enabled = true"
            " and topic_id is not null"
            " and ut.is_enabled = true"
            " and ut.topic_words_sequence != ''"
        " )"
        " UNION"
            " ("
            " select"
            " ulp.src_lang,"
            " ulp.dst_lang,"
            " ulp.mix_value as lang_mix_value,"
            " COALESCE(ugt.mix_value, 1) as mix_value,"
            " gt.default_position as position,"
            " ulp.last_enable_time as ulp_last_enable_time,"
            " gt.words as words_sequence"
            " from user_language_pairs as ulp"
            " left join global_topics as gt"
            " on gt.src_lang = ulp.src_lang"
            " left join user_global_topics as ugt"
            " on gt.topic_id = ugt.topic_id"
            " and gt.src_lang = ugt.src_lang"
            " and ugt.dst_lang = ulp.dst_lang"
            " and ugt.user_id = ulp.user_id"
            " where ulp.is_enabled = true"
            " and ulp.user_id = $1"
            " and ugt.is_enabled = true"
            " and ugt.topic_id is not null"
            " )"
        " order by ulp_last_enable_time asc, position asc", user_id
    );

    co_return result;
}

static drogon::Task<> create_translations_db_tables_for_all_langs(drogon::orm::DbClientPtr translations_db) {
    for (auto src_lang : local_constants::languages::ai_supported_languages_sorted_by_learning_popularity) {

        // create table for each src_lang
        auto banned_word_table_name = local_utils::translations_db::create_banned_word_table_name(src_lang);
        
        // hide notice
        co_await translations_db->execSqlCoro("SET client_min_messages TO WARNING");

        co_await translations_db->execSqlCoro(std::string("CREATE TABLE IF NOT EXISTS ") + banned_word_table_name 
            + " ( word TEXT PRIMARY KEY, time_added INT NOT NULL )"
        );

        // create index (time_added)
        co_await translations_db->execSqlCoro(std::string("CREATE INDEX IF NOT EXISTS idx_time_added_") + src_lang + " ON " + banned_word_table_name + " (time_added)");

        auto word_phrase_table_name = local_utils::translations_db::create_word_phrase_table_name(src_lang);
        // create table for each src_lang
        co_await translations_db->execSqlCoro(std::string("CREATE TABLE IF NOT EXISTS ") + word_phrase_table_name
            + "( id SERIAL NOT NULL,"
            " word TEXT NOT NULL,"
            " dst_lang CHAR(2) NOT NULL,"
            " phrase TEXT NOT NULL,"
            " word_translation TEXT NOT NULL,"
            " phrase_translation TEXT NOT NULL,"
            " PRIMARY KEY (word, dst_lang)"
            " ) PARTITION BY LIST (dst_lang)"
        );

        for (auto dst_lang : local_constants::languages::ai_supported_languages_sorted_by_learning_popularity) {
            if (src_lang == dst_lang) continue;

            // create partition table for each dst_lang
            auto partition_table_name = std::string("word_phrase_") + src_lang + "_" + dst_lang;
            co_await translations_db->execSqlCoro(std::string("CREATE TABLE IF NOT EXISTS ") + partition_table_name
                + " PARTITION OF " + word_phrase_table_name + " FOR VALUES IN ('" + dst_lang +"')"
            );

            // create index (word, dst_lang) for each dst_lang
            co_await translations_db->execSqlCoro(std::string("CREATE INDEX IF NOT EXISTS idx_word_phrase_") + src_lang + "_" + dst_lang + "_word_dst_lang ON "
                + partition_table_name + "(word, dst_lang)"
            );

            // create index (word_translation, dst_lang) for each dst_lang
            co_await translations_db->execSqlCoro(std::string("CREATE INDEX IF NOT EXISTS idx_word_phrase_") + src_lang + "_" + dst_lang + "_word_translation_dst_lang ON "
                + partition_table_name + "(word_translation, dst_lang)"
            );
        }
    }
}

static drogon::Task<std::pair<uint32_t, std::unordered_set<std::string>>> get_banned_words_for_src_lang_from_time_added(drogon::orm::DbClientPtr translations_db, const std::string& src_lang, uint32_t time_added = 0) {
    auto banned_word_table_name = local_utils::translations_db::create_banned_word_table_name(src_lang);

    auto result_rows = co_await translations_db->execSqlCoro("select word, time_added from " + banned_word_table_name + " where time_added >= $1", time_added);

    std::unordered_set<std::string> result_words;

    result_words.reserve(result_rows.size());

    uint32_t last_time_added = 0;

    for (auto& row : result_rows) {
        result_words.insert(row[0].as<std::string>());
        
        auto time_added = row[1].as<uint32_t>();
        if (time_added > last_time_added) {
            last_time_added = time_added;
        }
    }

    co_return {last_time_added, std::move(result_words)};
}

static drogon::Task<uint32_t> add_word_phrase_translation(drogon::orm::DbClientPtr translations_db, const std::string& src_lang, const std::string& dst_lang, const std::string& src_word, const std::string& dst_word, const std::string& src_phrase, const std::string& dst_phrase) {
    auto word_phrase_table_name = local_utils::translations_db::create_word_phrase_table_name(src_lang);

    auto result = co_await translations_db->execSqlCoro("insert into " + word_phrase_table_name 
        + " (word, dst_lang, phrase, word_translation, phrase_translation) values ($1, $2, $3, $4, $5)"
        " on conflict (word, dst_lang) do update set phrase=EXCLUDED.phrase, "
        "word_translation=EXCLUDED.word_translation, phrase_translation=EXCLUDED.phrase_translation returning id",
        src_word, dst_lang, src_phrase, dst_word, dst_phrase
    );

    co_return result[0]["id"].as<uint32_t>();
}

static drogon::Task<> add_banned_word(drogon::orm::DbClientPtr translations_db, const std::string& lang, const std::string& word) {
    auto banned_word_table_name = local_utils::translations_db::create_banned_word_table_name(lang);

    auto cur_time = local_utils::time::get_unix_timestamp();

    co_await translations_db->execSqlCoro("insert into " + banned_word_table_name 
        + " (word, time_added) values ($1, $2) on conflict (word) do nothing", word, cur_time
    );
}

static drogon::Task<bool> is_banned_word_exists(drogon::orm::DbClientPtr translations_db, const std::string& lang, const std::string& word) {
    auto banned_word_table_name = local_utils::translations_db::create_banned_word_table_name(lang);

    auto result = co_await translations_db->execSqlCoro("select 1 from " + banned_word_table_name + " where word = $1 limit 1", word);

    co_return result.size();
}

static drogon::Task<bool> does_user_have_subscription(drogon::orm::DbClientPtr users_db, uint64_t user_id) {
    auto result = co_await users_db->execSqlCoro("select 1 from user_google_play_subscription where user_id = $1 limit 1", user_id);

    if (result.size()) {
        co_return true;
    }

    bool is_user_tester = co_await is_user_of_sepcial_user_type(users_db, user_id, "tester");

    if (is_user_tester) {
        co_return true;
    }

    co_return false;
}
}