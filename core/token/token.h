#pragma once

#include <string>

namespace core::token {
    static constexpr uint32_t SECRET_PART_LEN = 6;

    std::string generate_random_string(size_t length);

    std::string create_device_refresh_token(uint32_t user_id, uint32_t device_num, const std::string& device_refresh_token);

    std::string create_device_token();

    // user_id, device_num, device_refresh_token
    std::tuple<uint32_t, uint32_t, std::string> decode_device_refresh_token(const std::string& refresh_token);

    std::string create_device_bare_token(uint32_t user_id, uint32_t device_num, const std::string& secret);

    // user_id, device_id
    std::tuple<uint32_t, uint32_t> decode_device_bare_token(const std::string& token, const std::string& secret);
}