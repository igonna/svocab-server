#include "sequence_generator.h"

namespace core::sequence_generation
{

void SequenceGenerator::add_user_words(std::string&& src_lang, std::unordered_map<std::string, core::progress_words::UserWordData> &&user_words) {
    user_words_by_src_lang.insert_or_assign(std::move(src_lang), std::move(user_words));
}

std::string SequenceGenerator::get_lang_pair_key(const std::string& src_lang, const std::string& dst_lang) {
    return src_lang + dst_lang;
}

void SequenceGenerator::add_lang_pair(const std::string& src_lang, const std::string& dst_lang, float mix_value) {
    auto key = get_lang_pair_key(src_lang, dst_lang);
    if (is_lang_pair_exists(key)) return;

    auto new_lang_pair_id = ++last_lang_pair_id;

    lang_pairs_by_id.insert({new_lang_pair_id, {src_lang, dst_lang}});
    lang_pairs_data_by_key.insert({key, {new_lang_pair_id, mix_value}});
}

bool SequenceGenerator::is_lang_pair_exists(const std::string& key) {
    return lang_pairs_data_by_key.contains(key);
}

inline std::optional<uint8_t> SequenceGenerator::get_lang_pair_id_for_src_lang(const std::string& src_lang) {
    for (auto& lang_pair : lang_pairs_by_id) {
        if (lang_pair.second.first == src_lang) {
            return lang_pair.first;
        }
    }
    return std::nullopt;
}

void SequenceGenerator::add_topic_words(std::string&& src_lang, std::string&& dst_lang, float lang_mix_value, float topic_mix_value, std::string&& topic_words_sequence) {
    std::vector<WordMixUnit> topic_words;

    auto lang_pair_key = get_lang_pair_key(src_lang, dst_lang);
    if (!is_lang_pair_exists(lang_pair_key)) {
        add_lang_pair(src_lang, dst_lang, lang_mix_value);
    }
    auto lang_pair_id = lang_pairs_data_by_key[lang_pair_key].first;

    // return true if added
    auto add_word = [&](std::string&& word) -> bool {
        if (!added_words_by_src_lang.contains(src_lang)) {
            added_words_by_src_lang[src_lang] = {};
        }

        auto& added_words = added_words_by_src_lang[src_lang];

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
        topic_words.push_back(WordMixUnit(lang_pair_id, 0, 0, std::move(word)));
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

    auto new_words_pack = WordsPack(
        topic_mix_value * local_constants::topics::topic_mix_value_multiplier,
        std::move(topic_words)
    );

    words_packs_by_lang_pairs_ids[lang_pair_id].push_back(std::move(new_words_pack));
}

void SequenceGenerator::add_to_repeat_user_words() {

    auto cur_time = local_utils::time::get_unix_timestamp();

    for (auto& lang_user_words : user_words_by_src_lang) {
        std::multimap<uint32_t, WordMixUnit, std::greater<uint32_t>> sorted_user_words;

        auto lang_pair_id_opt = get_lang_pair_id_for_src_lang(lang_user_words.first);
        if (!lang_pair_id_opt.has_value()) continue;
        auto lang_pair_id = lang_pair_id_opt.value();

        for (auto& user_word : lang_user_words.second) {
            if (!user_word.second.is_ready_for_repeat(cur_time)) {
                continue;
            }

            std::string word = user_word.first;
            auto new_word_mix_unit = WordMixUnit(lang_pair_id, user_word.second.confirmation_level, 
                user_word.second.last_update_time, std::move(word)
            );
            sorted_user_words.insert({user_word.second.last_update_time, std::move(new_word_mix_unit)});
        }

        uint32_t added_count = 0;
        std::vector<WordMixUnit> sorted_user_words_vec;
        for (auto& word : sorted_user_words) {
            if (++added_count > local_constants::max_user_sequence_words) {
                break;
            }
            sorted_user_words_vec.push_back(std::move(word.second));
        }

        auto new_words_pack = WordsPack(
            1.0f * local_constants::topics::topic_mix_value_multiplier,
            std::move(sorted_user_words_vec)
        );

        words_packs_by_lang_pairs_ids[lang_pair_id].push_back(std::move(new_words_pack));
    }

    user_words_by_src_lang.clear();
}

// std::vector<WordMixUnit> get_mixed_sequence(std::vector<WordsMixUnit>&& topics) {

// }

std::string SequenceGenerator::get_learning_sequence() {
    // added_words_by_src_lang.clear();

    std::string result;

    // auto to_repeat_user_words_by_lang = get_to_repeat_user_words_by_lang_pair();
    // topics_by_lang_pairs_ids;

    // // mixed_topics = mix topics
    // // mixed_lang = mix user words to mixed_topics
    // // mix langs

    // // UserWordsPack user_words_pack;

    // // lang_pair.vector.of.topic_mix_unit

    // std::vector<UserWordsPack> lang_pair_packs;
    // lang_pair_packs.reserve(topics_by_lang_pairs_ids.size());

    // for (auto& lang_pair : topics_by_lang_pairs_ids) {
    //     lang_pair.first.src_lang;
    //     lang_pair.first.dst_lang;
    //     lang_pair.first.mix_value;
    //     lang_pair.second;

    //     std::vector<UserWordsPack> topic_packs;
    //     topic_packs.reserve(lang_pair.second.size());

    //     for (auto& topic : lang_pair.second) {
    //         auto pack_size = topic.mix_value * local_constants::topics::topic_mix_value_multiplier;
    //         topic_packs.push_back(UserWordsPack(pack_size, std::move(topic.words)));
    //     }

    //     auto lang_sequence = mix_packs(topic_packs);
    //     auto pack_size = lang_pair.first.mix_value * local_constants::topics::lang_mix_value_multiplier;
    //     lang_pair_packs.push_back(UserWordsPack(pack_size, std::move(topic.words)));

    //     lang_pair.second.clear();
    // }

    // // std::cout << "learned words: " << std::endl;
    // // for (auto& user_words_lang : sorted_user_words_by_lang) {
    // //     std::cout << "src_l: " << user_words_lang.first << std::endl;

    // //     std::string user_words_as_str;
    // //     for (auto& user_word : user_words_lang.second) {
    // //         user_words_as_str += user_word.word + ';';
    // //     }
    // //     std::cout << user_words_as_str << std::endl;

    // }
    
    
    return result;
    //to_learn_word: src_lang;dst_lang;word;confirmation_level;is_passed
}
}