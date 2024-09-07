#include "token.h"

#include <regex>
#include <random>
#include <jwt-cpp/jwt.h>

#include "utils/constants.h"
#include "utils/utils.h"

namespace core::token {
    std::string generate_random_string(size_t length) {
        static const std::string characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

        std::random_device rd;
        std::mt19937 generator(rd());
        std::uniform_int_distribution<> distribution(0, characters.size() - 1);

        std::string result;
        for (size_t i = 0; i < length; ++i) {
            result += characters[distribution(generator)];
        }
    
        return result; 
    }

    std::string create_device_refresh_token(uint64_t user_id, uint32_t device_num, const std::string& device_refresh_token) {
        auto secret_1 = generate_random_string(SECRET_PART_LEN);
        auto secret_2 = generate_random_string(SECRET_PART_LEN);

        auto secret = secret_1 + secret_2;

        auto token = jwt::create()
            .set_type("JWS")
            .set_payload_claim("user_id", jwt::claim(std::to_string(user_id)))
            .set_payload_claim("device_num", jwt::claim(std::to_string(device_num)))
            .set_payload_claim("device_token", jwt::claim(device_refresh_token))
            .sign(jwt::algorithm::hs256{secret});

        return secret_1 + token + secret_2;
    }

    std::string create_device_token() {
        return generate_random_string(local_constants::device_token_length);
    }

    // user_id, device_num, device_refresh_token
    std::tuple<uint32_t, uint32_t, std::string> decode_device_refresh_token(const std::string& refresh_token) {
        uint32_t refresh_token_length = refresh_token.size();

        auto secret = refresh_token.substr(0, SECRET_PART_LEN) 
            + refresh_token.substr(refresh_token_length - SECRET_PART_LEN, SECRET_PART_LEN);
        auto token = refresh_token.substr(SECRET_PART_LEN, refresh_token_length - SECRET_PART_LEN - SECRET_PART_LEN);

        auto decoded = jwt::decode(token);
        auto verifier = jwt::verify().allow_algorithm(jwt::algorithm::hs256{secret});
        verifier.verify(decoded);

        auto user_id = std::stoi(decoded.get_payload_claim("user_id").as_string());
        auto device_num = std::stoi(decoded.get_payload_claim("device_num").as_string());
        auto device_token = decoded.get_payload_claim("device_token").as_string();

        return {user_id, device_num, device_token};
    }

    std::string create_device_bare_token(uint64_t user_id, uint32_t device_num, const std::string& secret) {
        auto token = jwt::create()
            .set_type("JWS")
            .set_payload_claim("user_id", jwt::claim(std::to_string(user_id)))
            .set_payload_claim("device_num", jwt::claim(std::to_string(device_num)))
            .set_expires_at(std::chrono::system_clock::now() + std::chrono::seconds{local_constants::secret_key_expire_seconds})
            .sign(jwt::algorithm::hs256{secret});

        return token;
    }

    // user_id, device_id
    std::tuple<uint64_t, uint32_t> decode_device_bare_token(const std::string& token, const std::string& secret) {
        auto decoded = jwt::decode(token);
        auto verifier = jwt::verify().allow_algorithm(jwt::algorithm::hs256{secret});
        verifier.verify(decoded);

        // Check if the token has expired
        auto exp_claim = decoded.get_payload_claim("exp");
        std::time_t exp_time = exp_claim.as_date().time_since_epoch().count();
        std::time_t time_now = local_utils::time::get_unix_timestamp();

        if (time_now >= exp_time) {
            throw std::runtime_error("token_expired");
        }

        auto user_id = std::stoi(decoded.get_payload_claim("user_id").as_string());
        auto device_num = std::stoi(decoded.get_payload_claim("device_num").as_string());

        return {user_id, device_num};
    }
}