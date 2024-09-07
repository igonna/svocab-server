#pragma once

namespace local_constants {
    static constexpr char users_db[] = "svocab_users";

    static constexpr char redis_db[] = "svocab";

    static constexpr uint8_t device_token_length = 16;

    static constexpr uint8_t code_max_requests = 3;
    static constexpr uint8_t verify_code_max_attempts = 3;
    static constexpr uint16_t block_code_request_seconds = 16*60;
    static constexpr uint16_t block_code_verify_attempts_seconds = 10*60;
    static constexpr uint16_t code_expire_seconds = 16*60;

    static constexpr uint32_t secret_key_expire_seconds = 60*60*2;


    static constexpr uint32_t title_max_length = 32;


namespace languages {
    static constexpr uint32_t lang_tag_size = 2;

    static constexpr auto chat_gpt_supported_languages_sorted_by_learning_popularity = {
        "en", "es", "fr", "de", "zh", "ja", "it", "pt", "ru", "ko", "ar", "nl", "sv", 
        "tr", "hi", "no", "el", "pl", "vi", "id", "da", "th", "uk", "fa", "cs", "fi", 
        "ro", "sw", "hu", "ca", "bn", "is", "ms", "sr", "hr", "hy", "ka", "sq", "bs", 
        "sk", "sl", "lt", "lv", "et", "mk", "ta", "te", "ur", "mr", "gu", "ml", "kk", 
        "mn", "my", "tl", "am", "kn", "so", "bg", "pa"
    };
}

namespace redis {
    static constexpr std::string secret_key_name = "secret";
    static constexpr uint8_t secret_key_length = 16;
}

namespace attributes {
    static constexpr std::string user_id = "user_id";
    static constexpr std::string device_num = "device_num";
}
    
}