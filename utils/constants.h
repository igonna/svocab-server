namespace local_constants {
    static constexpr char users_db[] = "svocab_users";

    static constexpr char redis_db[] = "svocab";

    static constexpr uint8_t register_max_attempts = 3;
    static constexpr uint16_t register_block_attempts_seconds = 10*60;
    static constexpr uint16_t register_code_expire_seconds = 15*60;
}