// core
#include "revoke.h"
#include "utils/constants.h"

namespace core::device {
    std::string create_revoke_device_key(uint32_t user_id, uint32_t device_num) {
        return std::string("revoke-") + std::to_string(user_id) + "-" + std::to_string(device_num);
    }

    drogon::Task<> revoke_device(drogon::nosql::RedisClientPtr rc, uint32_t user_id, uint32_t device_num) {
        auto revoke_key = create_revoke_device_key(user_id, device_num);

        auto result = co_await rc->execCommandCoro("set %s 0 %s %d",
            revoke_key.c_str(),
            "EX",
            local_constants::secret_key_expire_seconds
        );
        co_return;
    }

    drogon::Task<bool> is_device_revoked(drogon::nosql::RedisClientPtr rc, uint32_t user_id, uint32_t device_num) {
        auto revoke_key = create_revoke_device_key(user_id, device_num);
        auto result = co_await rc->execCommandCoro("exists %s", revoke_key.c_str());

        if (std::stoi(result.asString()) > 0) {
            co_return true;
        }
        co_return false;
    }
}