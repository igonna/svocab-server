#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include "utils/utils.h"
#include "core/progress_words/progress_words.h"
#include <numeric>

namespace core::sequence_generation
{
struct WordMixUnit {
    WordMixUnit(uint8_t lang_pair_id, uint8_t confirmation_level, uint32_t last_update_time, std::string&& word) 
        : word(std::move(word)), lang_pair_id(lang_pair_id), confirmation_level(confirmation_level), last_update_time(last_update_time) {}

    WordMixUnit(WordMixUnit&& other) : word(std::move(other.word)), lang_pair_id(other.lang_pair_id), 
        confirmation_level(other.confirmation_level), last_update_time(other.last_update_time) {}

    uint8_t lang_pair_id;
    uint8_t confirmation_level;
    uint32_t last_update_time;
    std::string word;
};

class WordsPack {
public:
	WordsPack(float pack_size, std::vector<WordMixUnit>&& words) : words(std::move(words)), pack_size(pack_size), overall_size(words.size()) {}
    WordsPack(WordsPack&& other) : words(std::move(other.words)), pack_size(other.pack_size), overall_size(other.overall_size) {}

	void get_pack(std::vector<WordMixUnit>& output) {
		cur_pack_size += pack_size;
		uint32_t export_pack_size = cur_pack_size;
		cur_pack_size -= static_cast<float>(export_pack_size);

		if (export_pack_size == 0) return;

		int end_pos = cur_pos + export_pack_size;

		if (end_pos > overall_size) end_pos = overall_size;

        for (auto it = words.begin() + cur_pos; it != words.begin() + end_pos; ++it) {
            output.push_back(std::move(*it)); 
        }

		cur_pos += export_pack_size;
	}

	void get_rest(std::vector<WordMixUnit>& output) {
		if (is_over()) return;
        int cur_pos_tmp = cur_pos;
        cur_pos = overall_size;

        for (auto it = words.begin() + cur_pos_tmp; it != words.end(); ++it) {
            output.push_back(std::move(*it));
        }
    }

    inline bool is_over() {
        return cur_pos >= overall_size;
    }

private:
	float pack_size;
	float cur_pack_size = 0.0;
	int cur_pos = 0;
	std::vector<WordMixUnit> words;
	int overall_size;
};

static std::vector<WordMixUnit> mix_packs(std::vector<WordsPack>& packs) {
	std::vector<int> ids(packs.size());
	std::iota(ids.begin(), ids.end(), 0);

	bool ids_updated = false;
	std::vector<WordMixUnit> result = {};
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

class SequenceGenerator {
public:
    void add_user_words(std::string&& src_lang, std::unordered_map<std::string, core::progress_words::UserWordData> &&user_words);
    void add_topic_words(std::string&& src_lang, std::string&& dst_lang, float lang_mix_value, float topic_mix_value, std::string&& topic_words_sequence);
    std::string get_learning_sequence();

private:
    void add_to_repeat_user_words();

    inline std::string get_lang_pair_key(const std::string& src_lang, const std::string& dst_lang);
    inline void add_lang_pair(const std::string& src_lang, const std::string& dst_lang, float mix_value);
    inline bool is_lang_pair_exists(const std::string& key);
    inline std::optional<uint8_t> get_lang_pair_id_for_src_lang(const std::string& src_lang);

private:
    std::unordered_map<std::string, std::unordered_map<std::string, core::progress_words::UserWordData>> user_words_by_src_lang;
    std::unordered_map<std::string, std::set<std::string>> added_words_by_src_lang;
    std::unordered_map<uint8_t, std::vector<WordsPack>> words_packs_by_lang_pairs_ids;

    std::unordered_map<std::string, std::pair<uint8_t, float>> lang_pairs_data_by_key;
    std::unordered_map<uint8_t, std::pair<std::string, std::string>> lang_pairs_by_id;
    uint8_t last_lang_pair_id = 0;
};
}