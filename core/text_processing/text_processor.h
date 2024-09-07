#pragma once

#include <unicode/unistr.h>

namespace core::text_processing
{
    inline icu::UnicodeString string_to_unicode(const std::string& str) {
        return icu::UnicodeString::fromUTF8(icu::StringPiece(str.data()));
    }

    std::string text_to_processed_words(icu::UnicodeString text, const char* language);
}