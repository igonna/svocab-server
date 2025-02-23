#pragma once
#include <drogon/plugins/Plugin.h>
#include <drogon/utils/coroutine.h>
#include <unordered_map>
#include <unicode/uchar.h>
#include <unicode/unistr.h>
#include <unicode/ucnv.h>

class FrequentWords : public drogon::Plugin<FrequentWords>
{
public:
	FrequentWords() {}
	void initAndStart(const Json::Value &config) override;
	void shutdown() override;

    uint32_t get_word_weight(std::string lang, icu::UnicodeString word) const;

private:
    const uint32_t first_word_weight = 10000000;

    // <lang<word, weight>>
    std::unordered_map<std::string, std::unordered_map<icu::UnicodeString, uint32_t>> words_weights;
};