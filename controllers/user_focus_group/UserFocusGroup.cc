#include "UserFocusGroup.h"
#include "utils/utils.h"
#include "core/postgres/queries.h"

namespace v1
{

class TopicMixer {
public:
    void add_topic(std::string src_lang, std::string dst_lang, float lang_mix_value, float topic_mix_value,
        std::string topic_words_sequence)
    {

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

Task<> UserFocusGroup::prepareFocusGroup(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    co_return local_utils::Response::OK(callback, "");
}

Task<> UserFocusGroup::getFocusGroup(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {

    // TO FINISH START

    // 1. generate words sequence for current topic (for view topic) - done

    // 1. generate words sequence for all topics (for mix value) - marked

    // 1. generate words sequence for current topic (for skip value) - done

    // 1. generate words sequence's for all topics (for main sequence) - done

    // 1. check if update required
    // 2. if yes, check on what topics
    // 3. generate words sequence for each topic, based on topic options
    // 4. generate final sequence
    // 5. update user sequence
    // 6. return focus group

    // TO FINISH END

    // how to mix learned and to_repeat ?

    // check_if_topics_updated -> means_new_generation_required
    // check_if_repeat_time_reached -> means_new_generation_required

    // min focus size && max focus size

    Json::Value result_focus_group;
    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        co_await core::sql::update_all_user_topics_sequences(users_db, user_id);

        // TODO: fetch global topics
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

    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::JSON_OK(callback, result_focus_group);
}

Task<> UserFocusGroup::getPreviousFocusGroupResult(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {

}

Task<> UserFocusGroup::getFocusGroupWithHiddenWord(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {

}

Task<> UserFocusGroup::updateWordPhase(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {

}

Task<> UserFocusGroup::completeFocusGroup(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {

}
}