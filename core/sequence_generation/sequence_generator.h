#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include "utils/utils.h"
#include "core/progress_words/progress_words.h"

namespace core::sequence_generation
{
struct LangPairMixUnit {
    LangPairMixUnit(std::string&& src_lang, std::string&& dst_lang, float mix_value) {
        this->src_lang = std::move(src_lang);
        this->dst_lang = std::move(src_lang);
        this->mix_value = mix_value;
    }

    LangPairMixUnit(LangPairMixUnit&& lang_mix_unit) {
        this->src_lang = std::move(lang_mix_unit.src_lang);
        this->dst_lang = std::move(lang_mix_unit.src_lang);
        this->mix_value = mix_value;
    }
    
    std::string src_lang;
    std::string dst_lang;
    float mix_value;
};

struct TopicMixUnit {
    TopicMixUnit(std::vector<std::string>&& words, float mix_value) {
        this->words = std::move(words);
        this->mix_value = mix_value;
    }

    TopicMixUnit(TopicMixUnit&& topic_mix_unit) {
        this->mix_value = topic_mix_unit.mix_value;
        this->words = std::move(topic_mix_unit.words);
    }

    std::vector<std::string> words;
    float mix_value;
};

class TopicMixer {
public:
    void add_user_words(std::string&& src_lang, std::unordered_map<std::string, core::progress_words::UserWordData> &&user_words);
    void add_topic_words(std::string&& src_lang, std::string&& dst_lang, float lang_mix_value, float topic_mix_value, std::string&& topic_words_sequence);
    std::string get_learning_sequence();

private:
    std::unordered_map<std::string, std::unordered_map<std::string, core::progress_words::UserWordData>> user_words_by_src_lang;
    std::unordered_map<std::string, std::set<std::string>> added_words_by_lang;

    std::vector<std::vector<std::string>> topics_words;

    std::vector<std::pair<LangPairMixUnit, std::vector<TopicMixUnit>>> topics_by_lang_pairs;
};
}