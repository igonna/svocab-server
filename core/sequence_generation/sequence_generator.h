#pragma once
#include <string>
#include <vector>

namespace core::sequence_generation
{
class SequenceGeneratorUnit {
public:
    // basicTopicCoef
    float mix_value;
    std::string sequence;
    SequenceGeneratorUnit() {}
};


class SequenceGenerator {
public:
    void add_sequence(std::string src_lang, std::string dst_lang, std::vector<std::string> words);
};
}