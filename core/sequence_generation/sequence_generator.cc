#include "sequence_generator.h"

class SequenceGeneratorUnit {
public:
    // basicTopicCoef
    float mix_value;
    std::string sequence;
    SequenceGeneratorUnit() {}
};

namespace core::sequence_generation
{

void TopicMixer::add_user_words(std::string&& src_lang, std::unordered_map<std::string, core::progress_words::UserWordData> &&user_words) {
    user_words_by_src_lang.insert_or_assign(std::move(src_lang), std::move(user_words));
}

void TopicMixer::add_topic_words(std::string&& src_lang, std::string&& dst_lang, float lang_mix_value, float topic_mix_value, std::string&& topic_words_sequence) {
    // std::vector<std::string> topic_words;

    // auto add_word = [&](std::string&& word) -> bool {
    //     if (!added_words_by_lang.contains(src_lang)) {
    //         added_words_by_lang[src_lang] = {};
    //     }

    //     auto& added_words = added_words_by_lang[src_lang];

    //     if (added_words.contains(word)) {
    //         return false;
    //     }

    //     auto user_lang_words_it = user_words_by_src_lang.find(src_lang);
    //     if (user_lang_words_it != user_words_by_src_lang.end()) {
    //         auto user_word_data_it = user_lang_words_it->second.find(word);
    //         if (user_word_data_it != user_lang_words_it->second.end()) {
    //             const auto& user_word_data = user_word_data_it->second;
    //             if (user_word_data.confirmation_level || user_word_data.is_hidden) {
    //                 return false; // word shouldn't be added
    //             }
    //         }
    //     }

    //     added_words.insert(word);
    //     topic_words.push_back(std::move(word));
    //     return true;
    // };

    // uint32_t added_words_count = 0;
    // std::string word;
    // for (auto c : topic_words_sequence) {
    //     if (c == ';') {
    //         if (add_word(std::move(word)) && ++added_words_count >= local_constants::max_user_sequence_words) {
    //             break;
    //         }
    //         word.clear();
    //         continue;
    //     }
    //     word += c;
    // }

    // if (word.size()) {
    //     add_word(std::move(word));
    // }

    // if (topic_words.empty()) return;

    // std::vector<TopicMixUnit>* lang_mix_units = nullptr;
    // auto topic_mix_unit = TopicMixUnit(std::move(topic_words), topic_mix_value);

    // for (auto& lang_pair_topics : topics_by_lang_pairs) {
    //     if (lang_pair_topics.first.src_lang == src_lang && lang_pair_topics.first.dst_lang == dst_lang) {
    //         lang_mix_units = &lang_pair_topics.second;
    //         lang_mix_units->push_back(std::move(topic_mix_unit));
    //     }
    // }

    // auto lang_pair_mix_unit = LangPairMixUnit(std::move(src_lang), std::move(dst_lang), lang_mix_value);

    // if (!lang_mix_units) {
    //     topics_by_lang_pairs.push_back({std::move(lang_pair_mix_unit), std::vector{std::move(topic_mix_unit)}});
    // }
}

// was cropped, learning sequence
std::string TopicMixer::get_learning_sequence() {
    //to_learn_word: src_lang;dst_lang;word;confirmation_level;is_passed
}
}