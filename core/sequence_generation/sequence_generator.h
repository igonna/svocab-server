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
struct LangPairMixUnit {
    LangPairMixUnit(std::string&& src_lang, std::string&& dst_lang, float mix_value)
        : src_lang(std::move(src_lang)), dst_lang(std::move(dst_lang)), mix_value(mix_value) {}

    LangPairMixUnit(LangPairMixUnit&& lang_mix_unit) noexcept 
        : src_lang(std::move(lang_mix_unit.src_lang)), dst_lang(std::move(lang_mix_unit.dst_lang)), mix_value(lang_mix_unit.mix_value) {}
    
    std::string src_lang;
    std::string dst_lang;
    float mix_value;
};

struct UserWordMixUnit {
    UserWordMixUnit(std::string&& word, core::progress_words::UserWordData user_word_data) : word(std::move(word)), data(user_word_data) {}
    UserWordMixUnit(UserWordMixUnit&& other) : word(std::move(other.word)), data(other.data) {}
    std::string word;
    core::progress_words::UserWordData data;
};

struct TopicMixUnit {
    TopicMixUnit(std::vector<UserWordMixUnit>&& words, float mix_value) : words(std::move(words)), mix_value(mix_value) {} 
    TopicMixUnit(TopicMixUnit&& topic_mix_unit) noexcept : words(std::move(topic_mix_unit.words)), mix_value(topic_mix_unit.mix_value) {} 

    std::vector<UserWordMixUnit> words;
    float mix_value;
};

class TopicMixer {
public:
    void add_user_words(std::string&& src_lang, std::unordered_map<std::string, core::progress_words::UserWordData> &&user_words);
    void add_topic_words(std::string&& src_lang, std::string&& dst_lang, float lang_mix_value, float topic_mix_value, std::string&& topic_words_sequence);
    std::string get_learning_sequence();

private:
    std::unordered_map<std::string, std::vector<UserWordMixUnit>> get_to_repeat_user_words_by_lang();

private:
    std::unordered_map<std::string, std::unordered_map<std::string, core::progress_words::UserWordData>> user_words_by_src_lang;
    std::unordered_map<std::string, std::set<std::string>> added_words_by_lang;
    std::vector<std::pair<LangPairMixUnit, std::vector<TopicMixUnit>>> topics_by_lang_pairs;
};





// class UserWordsPack {
// public:
// 	UserWordsPack(float pack_size, std::vector<UserWordMixUnit> words) : words(words), pack_size(pack_size), overall_size(words.size()) {}

// 	std::vector<UserWordMixUnit> get_pack() {
// 		cur_pack_size += pack_size;
// 		uint32_t export_pack_size = cur_pack_size;
// 		cur_pack_size -= static_cast<float>(export_pack_size);

// 		if (export_pack_size == 0) return {};

// 		int end_pos = cur_pos + export_pack_size;

// 		if (end_pos > overall_size) end_pos = overall_size;


// 		std::vector<UserWordMixUnit> export_data(words.begin() + cur_pos, words.begin() + end_pos);
// 		cur_pos += export_pack_size;

// 		return export_data;
// 	}

// 	std::vector<UserWordMixUnit> get_rest() {
// 		if (is_over()) return {};
//         int cur_pos_tmp = cur_pos;
//         cur_pos = overall_size;

//         return std::vector<UserWordMixUnit>(words.begin() + cur_pos_tmp, words.end());
//     }

//     bool is_over() {
//         return cur_pos >= overall_size;
//     }

// private:
// 	float pack_size;
// 	float cur_pack_size = 0.0;
// 	int cur_pos = 0;
// 	std::vector<UserWordMixUnit> words;
// 	int overall_size;
// };

// std::vector<UserWordMixUnit> create_result_list(std::vector<UserWordsPack> pack_list) {
// 	std::vector<int> ids(pack_list.size());
// 	std::iota(ids.begin(), ids.end(), 0);

// 	bool idsUpdated = false;
// 	std::vector<UserWordMixUnit> result = {};
// 	std::vector<int> ids_copy = ids;
// 	while (ids.size()) {
// 		if (idsUpdated) {
// 			ids_copy = ids;
// 			idsUpdated = false;
// 		}

// 		for (int id : ids_copy) {
// 			if (pack_list[id].is_over()) {
//                 ids.erase(std::remove(ids.begin(), ids.end(), id), ids.end());
// 				idsUpdated = true;
// 			} else {
// 				std::vector<UserWordMixUnit> words_to_add = ids.size() == 1 ? pack_list[id].get_rest() : pack_list[id].get_pack();
//                 result.insert(result.end(), words_to_add.begin(), words_to_add.end());
// 			}
// 		}
// 	}
// 	return result;
// }
}