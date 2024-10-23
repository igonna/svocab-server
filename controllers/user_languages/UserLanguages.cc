#include "UserLanguages.h"
#include "utils/utils.h"
#include "utils/constants.h"
#include "core/postgres/queries.h"
#include "core/sequence_generation/sequence_generator.h"

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

        auto last_enable_time = local_utils::time::get_unix_timestamp();
        
        {
            auto transaction = co_await users_db->newTransactionCoro();

            co_await transaction->execSqlCoro("insert into user_language_pairs (user_id, src_lang, dst_lang, is_enabled, last_enable_time) "
                "values ($1, $2, $3, $4, $5) on conflict (user_id, src_lang, dst_lang) do update "
                "set is_enabled = excluded.is_enabled, last_enable_time = excluded.last_enable_time", 
                user_id, src_lang, dst_lang, value, last_enable_time
            );

            co_await transaction->execSqlCoro("insert into user_words (user_id, src_lang) values ($1, $2)"
                " on conflict (user_id, src_lang) do nothing", user_id, src_lang
            );
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
    auto src_lang = req->getHeader("srcLang");
    auto dst_lang = req->getHeader("dstLang");

    if (src_lang.empty() || dst_lang.empty()) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    Json::Value result_body;
    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        core::sequence_generation::SequenceGenerator sequence_generator;

        // get user topics sequences
        {
            co_await core::sql::update_all_user_topics_sequences(users_db, user_id);

            auto topics_sequences_result = co_await core::sql::get_user_topics_sequences(users_db, user_id);

            for (auto& topic_sequence : topics_sequences_result) {
                auto src_lang = topic_sequence["src_lang"].as<std::string>();
                auto dst_lang = topic_sequence["dst_lang"].as<std::string>();
                auto lang_mix_value = topic_sequence["lang_mix_value"].as<float>();
                auto mix_value = topic_sequence["mix_value"].as<float>();
                auto words_sequence = topic_sequence["words_sequence"].as<std::string>();

                sequence_generator.add_topic_words(std::move(src_lang), std::move(dst_lang), lang_mix_value, mix_value, std::move(words_sequence));
            }
        }

        auto lang_pair_sequences = sequence_generator.get_words_sequences_by_lang_pair();

        result_body["mix_value_multiplier"] = local_constants::topics::lang_mix_value_multiplier;
        result_body["min_mix_value"] = local_constants::topics::min_mix_value;
        result_body["max_mix_value"] = local_constants::topics::max_mix_value;

        bool this_added = false;

        auto& language_pairs = result_body["lang_pairs_words"] = Json::arrayValue;
        for (auto& lang_pair_sequence : lang_pair_sequences) {
            auto& lang_pair_src_lang = lang_pair_sequence.src_lang;
            auto& lang_pair_dst_lang = lang_pair_sequence.dst_lang;

            bool is_this = false;
            if (lang_pair_src_lang == src_lang && lang_pair_dst_lang == dst_lang) {
                is_this = true;
                this_added = true;
            }

            language_pairs.append(createLangPairWordsJson(is_this, std::move(lang_pair_sequence.words), lang_pair_sequence.mix_value));
        }

        if (!this_added) {
            auto lang_pair_result = co_await users_db->execSqlCoro("select mix_value from user_language_pairs"
                " where user_id = $1 and src_lang = $2 and dst_lang = $3", user_id, src_lang, dst_lang
            );

            if (lang_pair_result.empty()) {
                co_return local_utils::Response::Error::ERROR(callback, k404NotFound, "lang_pair_not_exists");
            }

            auto mix_value = lang_pair_result[0]["mix_value"].as<float>();
            std::string no_words = "";
            language_pairs.append(createLangPairWordsJson(true, std::move(no_words), mix_value));
        }
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }
    
    co_return local_utils::Response::JSON_OK(callback, result_body);
}

Json::Value UserLanguages::createLangPairWordsJson(bool is_this, std::string&& words, float mix_value) {
    Json::Value new_lang_pair_json;

    new_lang_pair_json["this"] = is_this;
    new_lang_pair_json["words"] = std::move(words);
    new_lang_pair_json["mix_value"] = mix_value;
    
    return new_lang_pair_json;
}
}