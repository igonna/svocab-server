#pragma once
#include <drogon/plugins/Plugin.h>
#include <drogon/utils/coroutine.h>
#include <unordered_map>
#include <unicode/uchar.h>
#include <unicode/unistr.h>
#include <unicode/ucnv.h>
#include "utils/constants.h"
#include "utils/predicates.h"

using UnicodeStringHash = predicates::UnicodeStringHash;
using UnicodeStringEqual = predicates::UnicodeStringEqual;

enum class WordRarityCategory {
    common = 0,
    rare = 1,
    exotic = 2
};

struct WordsFrequencies {
    WordsFrequencies(std::unordered_map<icu::UnicodeString, uint32_t, UnicodeStringHash, UnicodeStringEqual>&& frequencies) : frequencies(frequencies) {}

    inline std::tuple<uint32_t, WordRarityCategory> get_frequency(const icu::UnicodeString& word) const {
        const auto& it = frequencies.find(word);

        if (it != frequencies.end()) {
            return {it->second, get_rarity_category(it->second)};
        }

        return {0, get_rarity_category(0)};
    }

private:
    inline WordRarityCategory get_rarity_category(uint32_t frequency) const {
        const static auto common_rarity_bound = local_constants::word_frequency::first_word_frequency - 12000;
        const static auto rare_rarity_bound = local_constants::word_frequency::first_word_frequency - 30000;

        if (frequency > common_rarity_bound) return WordRarityCategory::common;
        else if (frequency > rare_rarity_bound) return WordRarityCategory::rare;
        else return WordRarityCategory::exotic;
    }

private:
    WordsFrequencies() = delete;
    std::unordered_map<icu::UnicodeString, uint32_t, UnicodeStringHash, UnicodeStringEqual> frequencies;
};

class FrequentWords : public drogon::Plugin<FrequentWords>
{
public:
	FrequentWords() {}
	void initAndStart(const Json::Value &config) override;
	void shutdown() override {};

    const WordsFrequencies* get_words_frequencies_for_lang(const std::string& lang) const;

private:
    std::string frequent_words_dir;
    std::unordered_map<std::string, WordsFrequencies> languages_words_frequencies;
};