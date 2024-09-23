#include "UserFocusGroup.h"
#include "utils/utils.h"
#include "core/postgres/queries.h"

namespace v1
{

class TopicMixer {
public:
    void add_topic(std::string src_lang, std::string dst_lang, float lang_mix_value, float topic_mix_value,
        std::string topic_words_sequence
    ) {

    }

    void add_words_to_exclude(std::string src_lang, std::string dst_lang, std::unordered_set<std::string> words_to_exclude) {
        excluded_words_by_lang_pair.insert({
            local_utils::language_pair::create_lang_pair_key(src_lang, dst_lang), 
            words_to_exclude
        });
    }

private:
    std::unordered_map<std::string, std::unordered_set<std::string>> excluded_words_by_lang_pair;

    static constexpr float lang_mix_coef = 2.0;
    static constexpr float topic_mix_coef = 1.0;
};

Task<> UserFocusGroup::getFocusGroup(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    Json::Value result_focus_group;
    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        auto result = co_await users_db->execSqlCoro("select last_update_id, last_gen_for_repeat_time from user_sequence "
            "where user_id = $1 and last_gen_id != last_update_id", 
            user_id
        );

        auto sequence_last_update_id = result[0]["last_update_id"].as<uint32_t>();
        auto sequence_last_gen_for_repeat_time = result[0]["last_gen_for_repeat_time"].as<uint32_t>();

        // if generation required
        if (result.size()) {
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

            // add all topics words sequences to mixer
            auto topics_sequences_result = co_await users_db->execSqlCoro(
                "select ut.src_lang, ut.dst_lang, ulp.mix_value as lang_mix_value, ut.mix_value as topic_mix_value, ut.topic_words_sequence "
                "from user_topics as ut "
                "join user_language_pairs as ulp "
                "on ut.user_id = ulp.user_id "
                "and ut.src_lang = ulp.src_lang "
                "and ut.dst_lang = ulp.dst_lang "
                "where ulp.is_enabled = true "
                "and ulp.user_id = $1 and ut.last_update_id != 0 "
                "order by ulp.last_enable_time desc, ut.position asc",
                user_id
            );

            TopicMixer topic_mixer;

            for (auto& topic_sequence : topics_sequences_result) {
                auto src_lang = topic_sequence["src_lang"].as<std::string>();
                auto dst_lang = topic_sequence["dst_lang"].as<std::string>();
                auto lang_mix_value = topic_sequence["lang_mix_value"].as<float>();
                auto topic_mix_value = topic_sequence["topic_mix_value"].as<float>();
                auto topic_words_sequence = topic_sequence["topic_words_sequence"].as<std::string>();

                topic_mixer.add_topic(std::move(src_lang), std::move(dst_lang),
                    lang_mix_value, topic_mix_value, std::move(topic_words_sequence)
                );
            }

            float current_lang_mix_value = 1.0;
            float current_topic_mix_value = 1.0;

            uint32_t words_per_lang = 4;
            uint32_t words_per_topic = 2;

            std::vector<std::string> my_vector;
            auto& x = my_vector.at(0);

            // law: sum(words_per_topic) <= words_per_lang

            // for (auto& lang : langs) {
            //     for (auto& word : words) {

            //     }
            // }

            // array_1, mix_value_1

            // array_2, mix_value_2

            // lang_1_word_1, lang_1_word_2, lang_2_word_1, lang_2_word_2

            // [languages, mix_values] -> [words_sequences, mix_values]

            std::unordered_map<std::string, std::unordered_map<std::string, std::string>> topics_sequences;


            // std::vector<>

            for (auto& topic_sequence : topics_sequences_result) {
                
            }

            // sequence generation is just mix topics's sequences

            // fetch user topic_words_sequence's
            // and generate final sequence
        } else {
            // just fetch words from sequence
        }

    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::JSON_OK(callback, result_focus_group);
}

Task<> UserFocusGroup::updateFocusGroup(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    co_return local_utils::Response::OK(callback, "");
}

}