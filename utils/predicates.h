#pragma once

#include <unordered_map>
#include <unicode/uchar.h>
#include <unicode/unistr.h>
#include <unicode/ucnv.h>

namespace predicates {
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
}