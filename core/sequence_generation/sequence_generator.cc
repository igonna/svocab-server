#include "sequence_generator.h"

namespace core::sequence_generation
{

drogon::Task<> SequenceGenerator::add_user_words(std::string&& src_lang, std::unordered_map<std::string, core::progress_words::UserWordData> &&user_words) {
    if (!current_banned_words_by_src_lang.contains(src_lang)) {
        auto current_banned_words = co_await banned_words_plugin->get_current_banned_words_for_src_lang(src_lang);
        current_banned_words_by_src_lang.emplace(src_lang, std::move(current_banned_words));
    }
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
    lang_pairs_ids_in_order.push_back(new_lang_pair_id);
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

float SequenceGenerator::get_lang_pair_mix_value_by_id(uint8_t id) {
    auto& lang_pair = lang_pairs_by_id[id];
    auto lang_pair_key = get_lang_pair_key(lang_pair.first, lang_pair.second);
    return lang_pairs_data_by_key[lang_pair_key].second;
}

drogon::Task<> SequenceGenerator::add_topic_words(std::string&& src_lang, std::string&& dst_lang, float lang_mix_value, float topic_mix_value, std::string&& topic_words_sequence) {
    if (!current_banned_words_by_src_lang.contains(src_lang)) {
        auto current_banned_words = co_await banned_words_plugin->get_current_banned_words_for_src_lang(src_lang);
        current_banned_words_by_src_lang.emplace(src_lang, std::move(current_banned_words));
    }
    auto& current_banned_words = current_banned_words_by_src_lang.at(src_lang);

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
            if (current_banned_words.is_banned(word)) {
                word.clear();
                continue;
            }

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

    if (topic_words.empty()) co_return;

    auto new_words_pack = WordsPack(
        topic_mix_value * local_constants::topics::topic_mix_value_multiplier,
        std::move(topic_words)
    );

    words_packs_by_lang_pairs_ids[lang_pair_id].push_back(std::move(new_words_pack));
}

std::unordered_map<uint8_t, std::vector<WordMixUnit>> SequenceGenerator::get_words_to_repeat_by_lang_pair() {
    std::unordered_map<uint8_t, std::vector<WordMixUnit>> result;

    auto cur_time = local_utils::time::get_unix_timestamp();

    for (auto& lang_user_words : user_words_by_src_lang) {
        auto& current_banned_words = current_banned_words_by_src_lang.at(lang_user_words.first);

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

        if (sorted_user_words.empty()) continue;

        uint32_t added_count = 0;
        std::vector<WordMixUnit> sorted_user_words_vec;
        for (auto& word : sorted_user_words) {
            if (current_banned_words.is_banned(word.second.word)) {
                continue;
            }
            
            if (++added_count > local_constants::max_user_sequence_words) {
                break;
            }
            sorted_user_words_vec.push_back(std::move(word.second));
        }

        result[lang_pair_id] = std::move(sorted_user_words_vec);
    }

    user_words_by_src_lang.clear();
    return result;
}

static std::vector<WordMixUnit> get_mixed_words(std::vector<WordsPack>& packs) {
    std::vector<int> ids(packs.size());
	std::iota(ids.begin(), ids.end(), 0);

	bool ids_updated = false;
	std::vector<WordMixUnit> result;

	std::vector<int> ids_copy = ids;
	while (ids.size()) {
		if (ids_updated) {
			ids_copy = ids;
			ids_updated = false;
		}

		for (int id : ids_copy) {
			if (packs[id].is_over()) {
                ids.erase(std::remove(ids.begin(), ids.end(), id), ids.end());
				ids_updated = true;
			} else {
                if (ids.size() == 1) {
                    packs[id].get_rest(result);
                } else {
                    packs[id].get_pack(result);
                }
			}
		}
	}
	return result;
}

std::vector<WordsPack> SequenceGenerator::get_mixed_language_packs() {
    added_words_by_src_lang.clear();

    auto words_to_repeat_by_lang_pair = get_words_to_repeat_by_lang_pair();

    std::vector<WordsPack> to_mix_lang_packs;

    for (auto lang_pair_id : lang_pairs_ids_in_order) {
        if (!words_packs_by_lang_pairs_ids.contains(lang_pair_id)) continue;

        auto& lang_pair_packs = words_packs_by_lang_pairs_ids[lang_pair_id];

        auto lang_sequence = get_mixed_words(lang_pair_packs);

        if (words_to_repeat_by_lang_pair.contains(lang_pair_id)) {
            std::vector<WordsPack> to_mix_with_repeat;

            auto lang_sequence_pack = WordsPack(
                local_constants::topics::to_repeat_pack_size,
                std::move(lang_sequence)
            );
            lang_sequence.clear();

            auto& to_repeat_words = words_to_repeat_by_lang_pair[lang_pair_id];
            auto to_repeat_pack = WordsPack(
                local_constants::topics::to_repeat_pack_size,
                std::move(to_repeat_words)
            );

            to_mix_with_repeat.push_back(std::move(lang_sequence_pack));
            to_mix_with_repeat.push_back(std::move(to_repeat_pack));

            lang_sequence = get_mixed_words(to_mix_with_repeat);
        }

        auto lang_sequence_pack = WordsPack(
            get_lang_pair_mix_value_by_id(lang_pair_id) * local_constants::topics::lang_mix_value_multiplier,
            std::move(lang_sequence)
        );

        to_mix_lang_packs.push_back(std::move(lang_sequence_pack));
    }

    words_packs_by_lang_pairs_ids.clear();

    return to_mix_lang_packs;
}

std::vector<core::user_sequence_words::SequenceUserWord> SequenceGenerator::mixed_words_to_user_sequence_words(std::vector<core::sequence_generation::WordMixUnit>& mix_words) {
    std::vector<core::user_sequence_words::SequenceUserWord> result;
    result.reserve(mix_words.size());

    for (auto& word_mix_unit : mix_words) {
        const auto& lang_pair = lang_pairs_by_id[word_mix_unit.lang_pair_id];
        const auto& src_lang = lang_pair.first;
        const auto& dst_lang = lang_pair.second;

        result.push_back(core::user_sequence_words::SequenceUserWord(src_lang, dst_lang, std::move(word_mix_unit.word), word_mix_unit.confirmation_level, false, false));
    }

    mix_words.clear();

    return result;
}

std::string SequenceGenerator::get_learning_sequence_str() {
    auto to_mix_lang_packs = get_mixed_language_packs();

    auto mixed_words = get_mixed_words(to_mix_lang_packs);

    to_mix_lang_packs.clear();

    auto user_sequence_words = mixed_words_to_user_sequence_words(mixed_words);
    auto result = core::user_sequence_words::sequence_user_words_to_str(user_sequence_words);

    // user_sequence_words.clear();
    
    return result;
}

std::vector<LangPairMixData> SequenceGenerator::get_words_sequences_by_lang_pair() {
    auto to_mix_lang_packs = get_mixed_language_packs();

    std::vector<LangPairMixData> result;

    for (auto& lang_pack : to_mix_lang_packs) {
        std::vector<WordMixUnit> lang_pack_words;
        lang_pack.get_rest(lang_pack_words);

        std::string words_str;

        uint8_t lang_pair_id;

        for (auto& word : lang_pack_words) {
            lang_pair_id = word.lang_pair_id;
            words_str += word.word + ';';
        }

        if (words_str.empty()) continue;

        words_str.pop_back();

        auto lang_pair = lang_pairs_by_id[lang_pair_id];
        auto mix_value = get_lang_pair_mix_value_by_id(lang_pair_id);

        result.push_back(LangPairMixData(lang_pair.first, lang_pair.second, mix_value, std::move(words_str)));
    }

    return result;
}
}