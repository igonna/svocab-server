#include "FrequentWords.h"
#include <random>
#include <regex>
#include <unicode/unistr.h>
#include <unicode/normalizer2.h>
#include <drogon/HttpClient.h>
#include <curl/curl.h>
#include "utils/utils.h"

#include <filesystem>

namespace fs = std::filesystem;

using namespace drogon;

void FrequentWords::initAndStart(const Json::Value &config) {
    const std::string frequent_words_dir = "frequent_words";
    local_utils::file::get_string_from_file()

    fs::path current_directory = fs::current_path();

    // Loop through all files and directories in the current directory
    for (const auto& entry : fs::directory_iterator(current_directory)) {
        // Print the filename (path of each entry)
        if (fs::is_regular_file(entry)) { // Only list regular files, not directories
            std::cout << entry.path().filename() << std::endl;
        }
    }
    
}

void FrequentWords::shutdown() {}

uint32_t FrequentWords::get_word_weight(std::string lang, icu::UnicodeString word) const {

}
