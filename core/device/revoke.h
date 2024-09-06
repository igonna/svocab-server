#pragma once

#include <string>
#include <drogon/utils/coroutine.h>
#include <drogon/nosql/RedisClient.h>

namespace core::device {
    std::string create_revoke_device_key(uint32_t user_id, uint32_t device_num);

    drogon::Task<> revoke_device(drogon::nosql::RedisClientPtr rc, uint32_t user_id, uint32_t device_num);

    drogon::Task<bool> is_device_revoked(drogon::nosql::RedisClientPtr rc, uint32_t user_id, uint32_t device_num);
}