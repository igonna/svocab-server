#include "FrequentWords.h"
#include "utils/utils.h"
#include <filesystem>
#include "core/text_processing/text_processor.h"

using namespace drogon;

void FrequentWords::initAndStart(const Json::Value &config) {
    frequent_words_dir = config["frequent_words_dir"].asString();

    for (const auto& file : std::filesystem::directory_iterator(frequent_words_dir)) {
        auto file_content = local_utils::file::get_string_from_file(file.path());
        auto unicode_file_content = core::text_processing::string_to_unicode(file_content);
        file_content.clear();

        std::string language = file.path().filename();

        auto words = core::text_processing::get_unicode_words_from_text_as_vector(unicode_file_content, language.c_str());
        unicode_file_content.remove();

        std::unordered_map<icu::UnicodeString, uint32_t, 
            UnicodeStringHash, UnicodeStringEqual> words_frequencies;

        const auto words_count = words.size();

        for (uint32_t i = 0; i < words_count; ++i) {
            words_frequencies.insert({words[i], local_constants::word_frequency::first_word_frequency - i});
        }

        languages_words_frequencies.insert({std::move(language), std::move(words_frequencies)});
    }

    // add empty frequencies if some language data missing
    for (const auto& ai_lang : local_constants::languages::ai_supported_languages_sorted_by_learning_popularity) {
        if (!languages_words_frequencies.contains(ai_lang)) {
            std::unordered_map<icu::UnicodeString, uint32_t, 
                UnicodeStringHash, UnicodeStringEqual> empty_words_frequencies;

            languages_words_frequencies.insert({ai_lang, std::move(empty_words_frequencies)});
        }
    }

    LOG_INFO << "Frequent words initialized";
}

const WordsFrequencies* FrequentWords::get_words_frequencies_for_lang(const std::string& lang) const {
    auto it = languages_words_frequencies.find(lang);
    
    if (it != languages_words_frequencies.end()) {
        return &(it->second);
    }

    return nullptr;
}