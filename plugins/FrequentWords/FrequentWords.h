#pragma once
#include <drogon/plugins/Plugin.h>
#include <drogon/utils/coroutine.h>
#include <unordered_map>
#include <unicode/uchar.h>
#include <unicode/unistr.h>
#include <unicode/ucnv.h>

struct UnicodeStringHash {
    std::size_t operator()(const icu::UnicodeString& str) const {
        return std::hash<std::u16string>()(std::u16string(str.getBuffer(), str.length()));
    }
};

struct UnicodeStringEqual {
    bool operator()(const icu::UnicodeString& lhs, const icu::UnicodeString& rhs) const {
        return lhs == rhs;
    }
};

struct WordsFrequencies {
    WordsFrequencies(std::unordered_map<icu::UnicodeString, uint32_t, UnicodeStringHash, UnicodeStringEqual>&& frequencies) : frequencies(frequencies) {}

    inline uint32_t get_frequency(const icu::UnicodeString& word) const {
        const auto& it = frequencies.find(word);

        if (it != frequencies.end()) {
            return it->second;
        }

        return 0;
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
    const uint32_t first_word_frequency = 10000000;
    std::unordered_map<std::string, WordsFrequencies> languages_words_frequencies;
};