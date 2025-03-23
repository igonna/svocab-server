#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include "utils/utils.h"
#include "core/progress_words/progress_words.h"
#include <numeric>
#include "utils/constants.h"
#include "core/user_sequence_words/user_sequence_words.h"
#include "plugins/BannedWords/BannedWords.h"

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
	WordsPack(float pack_size, std::vector<WordMixUnit>&& words) : words(std::move(words)), pack_size(pack_size) {
        uint32_t words_size = this->words.size();
        overall_size = words_size > local_constants::max_user_sequence_words ? local_constants::max_user_sequence_words : words_size;
    }
    WordsPack(WordsPack&& other) : words(std::move(other.words)), pack_size(other.pack_size), overall_size(other.overall_size) {}

	void get_pack(std::vector<WordMixUnit>& output) {
        cur_pack_size += pack_size;
        if (static_cast<uint32_t>(cur_pack_size) == 0) return;

        uint32_t export_pack_size = static_cast<uint32_t>(cur_pack_size);
        cur_pack_size -= export_pack_size;

        uint32_t end_pos = cur_pos + export_pack_size;
        if (end_pos > overall_size) {
            end_pos = overall_size;
        }

        for (uint32_t i = cur_pos; i < end_pos; ++i) {
            output.push_back(std::move(words[i]));
        }

        cur_pos += export_pack_size;
	}

	void get_rest(std::vector<WordMixUnit>& output) {
		if (is_over()) return;
        auto cur_pos_tmp = cur_pos;
        cur_pos = overall_size;

        for (uint32_t i = cur_pos_tmp; i < overall_size; ++i) {
            output.push_back(std::move(words[i]));
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

struct LangPairMixData {
    LangPairMixData(const std::string& src_lang, const std::string& dst_lang, float mix_value, const std::string&& words) 
        : src_lang(src_lang), dst_lang(dst_lang), mix_value(mix_value), words(std::move(words)) {}

    LangPairMixData(LangPairMixData&& other) 
        : src_lang(std::move(other.src_lang)), dst_lang(std::move(other.dst_lang)), mix_value(other.mix_value), words(std::move(other.words)) {}

    std::string src_lang;
    std::string dst_lang;
    float mix_value;
    std::string words;
};

class SequenceGenerator {
public:
    SequenceGenerator(BannedWords* banned_words_plugin) : banned_words_plugin(banned_words_plugin) {}

    drogon::Task<> add_user_words(std::string&& src_lang, std::unordered_map<std::string, core::progress_words::UserWordData> &&user_words);
    drogon::Task<> add_topic_words(std::string&& src_lang, std::string&& dst_lang, float lang_mix_value, float topic_mix_value, std::string&& topic_words_sequence);
    static std::string get_learning_sequence_str(const std::vector<core::user_sequence_words::SequenceUserWord>& user_words_sequence);
    std::vector<LangPairMixData> get_words_sequences_by_lang_pair();
    std::vector<core::user_sequence_words::SequenceUserWord> generate();

private:
    std::vector<WordsPack> get_mixed_language_packs();
    std::unordered_map<uint8_t, std::vector<WordMixUnit>> get_words_to_repeat_by_lang_pair();

    inline std::string get_lang_pair_key(const std::string& src_lang, const std::string& dst_lang);
    inline void add_lang_pair(const std::string& src_lang, const std::string& dst_lang, float mix_value);
    inline bool is_lang_pair_exists(const std::string& key);
    inline std::optional<uint8_t> get_lang_pair_id_for_src_lang(const std::string& src_lang);
    inline float get_lang_pair_mix_value_by_id(uint8_t id);
    std::vector<core::user_sequence_words::SequenceUserWord> mixed_words_to_user_sequence_words(std::vector<core::sequence_generation::WordMixUnit>& mix_words);

private:
    BannedWords* banned_words_plugin;
    std::unordered_map<std::string, CurrentBannedWords> current_banned_words_by_src_lang;

    std::unordered_map<std::string, std::unordered_map<std::string, core::progress_words::UserWordData>> user_words_by_src_lang;
    std::unordered_map<std::string, std::set<std::string>> added_words_by_src_lang;
    std::unordered_map<uint8_t, std::vector<WordsPack>> words_packs_by_lang_pairs_ids;

    std::unordered_map<std::string, std::pair<uint8_t, float>> lang_pairs_data_by_key;
    std::unordered_map<uint8_t, std::pair<std::string, std::string>> lang_pairs_by_id;
    std::vector<uint8_t> lang_pairs_ids_in_order;
    uint8_t last_lang_pair_id = 0;
};
}