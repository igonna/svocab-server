#pragma once

#include "plugins/OpenaiGeneration/OpenaiGeneration.h"
#include "plugins/FrequentWords/FrequentWords.h"
#include "core/user_sequence_words/user_sequence_words.h"
#include <drogon/utils/coroutine.h>
#include <cstdint>
#include <vector>
#include <utility>

namespace core::phrases_generator {

struct FocusGroupPhrase {
    FocusGroupPhrase(
        GeneratedPhrase&& generated_phrase,
        std::vector<std::string>&& guess_words,
        std::vector<std::string>&& guess_letters,
        uint8_t confirmation_level
    ) : generated_phrase(std::move(generated_phrase)),
        guess_words(std::move(guess_words)),
        guess_letters(std::move(guess_letters)),
        confirmation_level(confirmation_level) {}

    FocusGroupPhrase(FocusGroupPhrase&& other) : generated_phrase(std::move(other.generated_phrase)),
        guess_words(std::move(other.guess_words)), guess_letters(std::move(other.guess_letters)), confirmation_level(other.confirmation_level) {}

    FocusGroupPhrase& operator=(FocusGroupPhrase&& other) {
		if (&other != this) {
			generated_phrase = std::move(other.generated_phrase);
			guess_words = std::move(other.guess_words);
			guess_letters = std::move(other.guess_letters);
            confirmation_level = other.confirmation_level;
		}

		return *this;
	}

    GeneratedPhrase generated_phrase;
    std::vector<std::string> guess_words;
    std::vector<std::string> guess_letters;
    uint8_t confirmation_level;
};

class PhrasesGenerator {
public:
    PhrasesGenerator (OpenaiGeneration *openai_phrase_generation, FrequentWords *frequent_words_plugin, drogon::orm::DbClientPtr translations_db) 
        : openai_phrase_generation(openai_phrase_generation), frequent_words_plugin(frequent_words_plugin), translations_db(translations_db) {}
    PhrasesGenerator() = delete;

    void add_phrase(core::user_sequence_words::SequenceUserWord &user_word, bool user_has_subscription);

    // could throw systems_are_busy_exception
    // if word not valid sets: user_word_ref->is_passed = true
    drogon::Task<std::vector<FocusGroupPhrase>> get_generated_phrases();

    uint32_t count() const;

    drogon::Task<FocusGroupPhrase> generate_phrase_with_words_and_letters(core::user_sequence_words::SequenceUserWord &user_word, bool user_has_subscription);

private:
    std::vector<std::pair<
        core::user_sequence_words::SequenceUserWord*,
        drogon::Task<FocusGroupPhrase>
    >> phrases_to_generate;

    OpenaiGeneration *openai_phrase_generation;
    FrequentWords *frequent_words_plugin;
    drogon::orm::DbClientPtr translations_db;
};

};