#pragma once

#include <unordered_set>
#include <cstdint>
#include <unordered_map>
#include <string>

namespace local_constants {
    static constexpr char users_db[] = "svocab_users";
    static constexpr char devices_info_db[] = "devices_info";
    static constexpr char translations_db[] = "svocab_translations";

    static constexpr char redis_db[] = "svocab";

    static constexpr uint8_t device_token_length = 16;

    static constexpr uint8_t register_max_requests = 100;
    static constexpr uint8_t code_max_requests = 3;
    static constexpr uint8_t verify_code_max_attempts = 3;
    static constexpr uint16_t block_register_request_seconds = 10;
    static constexpr uint16_t block_code_request_seconds = 16*60;
    static constexpr uint16_t block_code_verify_attempts_seconds = 10*60;
    static constexpr uint16_t code_expire_seconds = 16*60;

    static constexpr uint32_t secret_key_expire_seconds = 60*60*2;
    static constexpr uint32_t devices_info_update_key_expire_seconds = 60*60*24;
    static constexpr uint32_t global_rank_update_key_expire_seconds = 5*60;

    static constexpr uint32_t focus_group_update_lock_key_expire_seconds = 8;

    static constexpr uint32_t title_max_length = 30;

    static constexpr uint32_t first_phase_repeat_time_interval_in_seconds = 6*60*60;

    static constexpr uint16_t languages_str_expire_sconds = 3*60;

    static constexpr uint32_t confirmation_level_1_repeat_timeout_seconds = 60*60*3;
    static constexpr uint32_t confirmation_level_2_repeat_timeout_seconds = 60*60*24;
    static constexpr uint32_t confirmation_level_3_repeat_timeout_seconds = 60*60*72;

    static constexpr float compress_user_words_on_percent = 0.2f;

    static constexpr uint32_t max_user_sequence_words = 200;

    static constexpr uint16_t openai_free_users_limit_key_expire_seconds = 60;

    static constexpr uint32_t user_blocked_focus_group_generation_seconds = 60*3;

namespace word_frequency {
    const uint32_t first_word_frequency = 10000000;
}

namespace languages {
    static constexpr uint32_t lang_tag_size = 2;

    static const std::unordered_set<std::string> ai_supported_languages_sorted_by_learning_popularity = {
        "en", "es", "fr", "de", "zh", "ja", "it", "pt", "ru", "ko", "ar", "nl", "sv", 
        "tr", "hi", "no", "el", "pl", "vi", "id", "da", "th", "uk", "fa", "cs", "fi", 
        "ro", "sw", "hu", "ca", "bn", "is", "ms", "sr", "hr", "hy", "ka", "sq", "bs", 
        "sk", "sl", "lt", "lv", "et", "mk", "ta", "te", "ur", "mr", "gu", "ml", "kk", 
        "mn", "my", "tl", "am", "kn", "so", "bg", "pa"
    };

    // ISO 639-1 to ISO 3166-1 alpha-2
    static const std::unordered_map<std::string, const char*> language_to_country = {
        {"en", "US"},  // English -> United States
        {"es", "ES"},  // Spanish -> Spain
        {"fr", "FR"},  // French -> France
        {"de", "DE"},  // German -> Germany
        {"zh", "CN"},  // Chinese -> China
        {"ja", "JP"},  // Japanese -> Japan
        {"it", "IT"},  // Italian -> Italy
        {"pt", "PT"},  // Portuguese -> Portugal
        {"ru", "RU"},  // Russian -> Russia
        {"ko", "KR"},  // Korean -> South Korea
        {"ar", "SA"},  // Arabic -> Saudi Arabia
        {"nl", "NL"},  // Dutch -> Netherlands
        {"sv", "SE"},  // Swedish -> Sweden
        {"tr", "TR"},  // Turkish -> Turkey
        {"hi", "IN"},  // Hindi -> India
        {"no", "NO"},  // Norwegian -> Norway
        {"el", "GR"},  // Greek -> Greece
        {"pl", "PL"},  // Polish -> Poland
        {"vi", "VN"},  // Vietnamese -> Vietnam
        {"id", "ID"},  // Indonesian -> Indonesia
        {"da", "DK"},  // Danish -> Denmark
        {"th", "TH"},  // Thai -> Thailand
        {"uk", "UA"},  // Ukrainian -> Ukraine
        {"fa", "IR"},  // Persian -> Iran
        {"cs", "CZ"},  // Czech -> Czech Republic
        {"fi", "FI"},  // Finnish -> Finland
        {"ro", "RO"},  // Romanian -> Romania
        {"sw", "SE"},  // Swahili -> Sweden
        {"hu", "HU"},  // Hungarian -> Hungary
        {"ca", "ES"},  // Catalan -> Spain
        {"bn", "BD"},  // Bengali -> Bangladesh
        {"is", "IS"},  // Icelandic -> Iceland
        {"ms", "MY"},  // Malay -> Malaysia
        {"sr", "RS"},  // Serbian -> Serbia
        {"hr", "HR"},  // Croatian -> Croatia
        {"hy", "AM"},  // Armenian -> Armenia
        {"ka", "GE"},  // Georgian -> Georgia
        {"sq", "AL"},  // Albanian -> Albania
        {"bs", "BA"},  // Bosnian -> Bosnia and Herzegovina
        {"sk", "SK"},  // Slovak -> Slovakia
        {"sl", "SI"},  // Slovenian -> Slovenia
        {"lt", "LT"},  // Lithuanian -> Lithuania
        {"lv", "LV"},  // Latvian -> Latvia
        {"et", "EE"},  // Estonian -> Estonia
        {"mk", "MK"},  // Macedonian -> North Macedonia
        {"ta", "IN"},  // Tamil -> India
        {"te", "IN"},  // Telugu -> India
        {"ur", "PK"},  // Urdu -> Pakistan
        {"mr", "IN"},  // Marathi -> India
        {"gu", "IN"},  // Gujarati -> India
        {"ml", "IN"},  // Malayalam -> India
        {"kk", "KZ"},  // Kazakh -> Kazakhstan
        {"mn", "MN"},  // Mongolian -> Mongolia
        {"my", "MM"},  // Burmese -> Myanmar
        {"tl", "PH"},  // Tagalog -> Philippines
        {"am", "ET"},  // Amharic -> Ethiopia
        {"kn", "IN"},  // Kannada -> India
        {"so", "SO"},  // Somali -> Somalia
        {"bg", "BG"},  // Bulgarian -> Bulgaria
        {"pa", "IN"}   // Punjabi -> India
    };
}

namespace redis {
    static const std::string secret_key_name = "secret";
    static constexpr uint8_t secret_key_length = 16;
    static const std::string devices_info_update_key_name = "devices_info_update";
    static const std::string global_rank_update_key_name = "global_rank_update";
    static const std::string openai_free_users_limit_key_name = "openai_free_users_limit";
}

namespace attributes {
    static const std::string user_id = "user_id";
    static const std::string device_num = "device_num";
}

namespace topics {
    static constexpr float to_repeat_pack_size = 3.0f;

    static constexpr float lang_mix_value_multiplier = 2.0f;
    static constexpr float topic_mix_value_multiplier = 1.0f;
    static constexpr float min_mix_value = 0.5f;
    static constexpr float mix_value = 1.0f;
    static constexpr float max_mix_value = 2.0f;

    static constexpr float min_skip_value = 0.0f;
    static constexpr float skip_value = 0.0f;
    static constexpr float max_skip_value = 1.0f;
}

namespace user_generation_limit {
    static constexpr uint32_t generation_limit_reset_seconds = 60*60*3;
    static constexpr uint32_t generation_limit_free_user = 16;
    static constexpr uint32_t generation_limit_subscriber_user = 320;
}

namespace user_file_processing_limit {
    static constexpr uint32_t file_processing_limit_reset_seconds = 60*60*3;
    static constexpr uint32_t file_processing_limit_free_user = 2;
    static constexpr uint32_t file_processing_limit_subscriber_user = 20;
};

namespace user_topic_generation_limit {
    static constexpr uint32_t topic_generation_limit_reset_seconds = 60*60*3;
    static constexpr uint32_t topic_generation_limit_free_user = 3;
    static constexpr uint32_t topic_generation_limit_subscriber_user = 20;
};

namespace focus_group {
    static constexpr uint8_t min_focus_group_size = 2;
    static constexpr uint8_t max_focus_group_size = 6;

    static constexpr uint32_t focus_group_expire_seconds = 60*5;
}

namespace statistics {
    static constexpr uint32_t learned_this_time_expire_seconds = 60*10;
}
}