#pragma once

#include "plugins/OpenaiPhraseGeneration/OpenaiPhraseGeneration.h"
#include "core/user_sequence_words/user_sequence_words.h"
#include <drogon/utils/coroutine.h>
#include <cstdint>
#include <vector>
#include <utility>

namespace core::phrases_generator {

class PhrasesGenerator {
public:
    PhrasesGenerator (OpenaiPhraseGeneration *openai_phrase_generation, drogon::orm::DbClientPtr translations_db) 
        : openai_phrase_generation(openai_phrase_generation), translations_db(translations_db) {}
    PhrasesGenerator() = delete;

    void add_phrase(core::user_sequence_words::SequenceUserWord &user_word, bool user_has_subscription);

    // could throw systems_are_busy_exception
    // if word not valid sets: user_word_ref->is_excluded = true
    drogon::Task<std::vector<GeneratedPhrase>> get_generated_phrases();

    uint32_t count() const;

private:
    std::vector<std::pair<
        core::user_sequence_words::SequenceUserWord*,
        drogon::Task<GeneratedPhrase>
    >> phrases_to_generate;

    OpenaiPhraseGeneration *openai_phrase_generation;
    drogon::orm::DbClientPtr translations_db;
};

};