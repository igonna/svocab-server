namespace local_constants {
    static constexpr char users_db[] = "svocab_users";

    static constexpr char redis_db[] = "svocab";

    static constexpr uint8_t link_code_max_requests = 3;
    static constexpr uint8_t link_max_attempts = 3;
    static constexpr uint16_t block_link_code_request_seconds = 16*60;
    static constexpr uint16_t block_link_attempts_seconds = 10*60;
    static constexpr uint16_t link_code_expire_seconds = 16*60;
}