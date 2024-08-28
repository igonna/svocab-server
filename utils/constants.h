#pragma once

namespace local_constants {
    static constexpr char users_db[] = "svocab_users";

    static constexpr char redis_db[] = "svocab";

    static constexpr uint8_t device_token_length = 16;

    static constexpr uint8_t link_code_max_requests = 3;
    static constexpr uint8_t link_max_attempts = 3;
    static constexpr uint16_t block_link_code_request_seconds = 16*60;
    static constexpr uint16_t block_link_attempts_seconds = 10*60;
    static constexpr uint16_t link_code_expire_seconds = 16*60;

    static constexpr uint32_t secret_key_expire_seconds = 60*60*2;

namespace redis {
    static constexpr std::string secret_key_name = "secret";
    static constexpr uint8_t secret_key_length = 16;
}

namespace attributes {
    static constexpr std::string user_id = "user_id";
    static constexpr std::string device_num = "device_num";
}
    
}