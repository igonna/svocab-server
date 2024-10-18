#include "sequence_generator.h"

namespace core::sequence_generation
{

void TopicMixer::add_user_words(std::string&& src_lang, std::unordered_map<std::string, core::progress_words::UserWordData> &&user_words) {
    user_words_by_src_lang.insert_or_assign(std::move(src_lang), std::move(user_words));
}

void TopicMixer::add_topic_words(std::string&& src_lang, std::string&& dst_lang, float lang_mix_value, float topic_mix_value, std::string&& topic_words_sequence) {
    std::vector<UserWordMixUnit> topic_words;

    // return true if added
    auto add_word = [&](std::string&& word) -> bool {
        if (!added_words_by_lang.contains(src_lang)) {
            added_words_by_lang[src_lang] = {};
        }

        auto& added_words = added_words_by_lang[src_lang];

        // don't add word if another topic have it
        if (added_words.contains(word)) {
            return false;
        }

        // find if user_word exists
        auto user_lang_words_it = user_words_by_src_lang.find(src_lang);
        if (user_lang_words_it != user_words_by_src_lang.end()) {
            auto user_word_data_it = user_lang_words_it->second.find(word);
            if (user_word_data_it != user_lang_words_it->second.end()) {
                const auto& user_word_data = user_word_data_it->second;
                // don't add word if word learned OR hidded
                if (user_word_data.confirmation_level > 0 || user_word_data.is_hidden) {
                    return false;
                }
            }
        }

        added_words.insert(word);
        UserWordMixUnit user_word_mix_unit(std::move(word), core::progress_words::UserWordData(0, false, 0));
        topic_words.push_back(std::move(user_word_mix_unit));
        return true;
    };

    uint32_t added_words_count = 0;
    std::string word;
    for (auto c : topic_words_sequence) {
        if (c == ';') {
            if (add_word(std::move(word)) && ++added_words_count >= local_constants::max_user_sequence_words) {
                break;
            }
            word.clear();
            continue;
        }
        word += c;
    }

    if (word.size()) {
        add_word(std::move(word));
    }

    if (topic_words.empty()) return;

    std::vector<TopicMixUnit>* lang_mix_units = nullptr;
    auto topic_mix_unit = TopicMixUnit(std::move(topic_words), topic_mix_value);

    // add to existing lang_pair
    for (auto& lang_pair_topics : topics_by_lang_pairs) {
        if (lang_pair_topics.first.src_lang == src_lang && lang_pair_topics.first.dst_lang == dst_lang) {
            lang_mix_units = &lang_pair_topics.second;
            lang_mix_units->push_back(std::move(topic_mix_unit));
        }
    }

    // add new lang_pair if not exists
    if (!lang_mix_units) {
        auto lang_pair_mix_unit = LangPairMixUnit(std::move(src_lang), std::move(dst_lang), lang_mix_value);
        std::vector<TopicMixUnit> topic_mix_unit_vec;
        topic_mix_unit_vec.push_back(std::move(topic_mix_unit));
        topics_by_lang_pairs.push_back({std::move(lang_pair_mix_unit), std::move(topic_mix_unit_vec)});
    }
}

std::unordered_map<std::string, std::vector<UserWordMixUnit>> TopicMixer::get_to_repeat_user_words_by_lang() {
    std::unordered_map<std::string, std::vector<UserWordMixUnit>> to_repeat_user_words_by_lang;

    auto cur_time = local_utils::time::get_unix_timestamp();

    for (auto& lang_user_words : user_words_by_src_lang) {
        std::multimap<uint32_t, UserWordMixUnit> sorted_user_words;
        uint32_t added_count = 0;

        for (auto& user_word : lang_user_words.second) {
            if (!user_word.second.is_ready_for_learn(cur_time)) {
                continue;
            }

            if (++added_count > local_constants::max_user_sequence_words) {
                break;
            }
            std::string word = user_word.first;
            auto new_user_word_mix_unit = UserWordMixUnit(std::move(word), user_word.second);
            sorted_user_words.insert({user_word.second.last_update_time, std::move(new_user_word_mix_unit)});
        }

        std::vector<UserWordMixUnit> sorted_user_words_vec;
        for (auto& word : sorted_user_words) {
            sorted_user_words_vec.push_back(std::move(word.second));
        }

        to_repeat_user_words_by_lang.insert({lang_user_words.first, std::move(sorted_user_words_vec)});
    }

    user_words_by_src_lang.clear();

    return to_repeat_user_words_by_lang;
}

std::string TopicMixer::get_learning_sequence() {
    added_words_by_lang.clear();

    std::string result;

    auto to_repeat_user_words_by_lang = get_to_repeat_user_words_by_lang();
    topics_by_lang_pairs;

    // mixed_topics = mix topics
    // mixed_lang = mix user words to mixed_topics
    // mix langs

    // UserWordsPack user_words_pack;

    for (auto& lang_pair : topics_by_lang_pairs) {
        lang_pair.first.src_lang;
        lang_pair.first.dst_lang;
        lang_pair.first.mix_value;
        lang_pair.second;
        
        for (auto& topic : lang_pair.second) {

            std::string words_as_str;
            for (auto& word : topic.words) {
                words_as_str += word.word + ';';
            }
        }

        lang_pair.second.clear();
    }

    // std::cout << "learned words: " << std::endl;
    // for (auto& user_words_lang : sorted_user_words_by_lang) {
    //     std::cout << "src_l: " << user_words_lang.first << std::endl;

    //     std::string user_words_as_str;
    //     for (auto& user_word : user_words_lang.second) {
    //         user_words_as_str += user_word.word + ';';
    //     }
    //     std::cout << user_words_as_str << std::endl;

    // }
    
    
    return result;
    //to_learn_word: src_lang;dst_lang;word;confirmation_level;is_passed
}
}