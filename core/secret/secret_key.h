#include <drogon/orm/DbClient.h>
#include <drogon/utils/coroutine.h>
#include "core/token/token.h"

namespace core::secret {
    static drogon::Task<std::string> get_secret_key(drogon::nosql::RedisClientPtr rc) {
        auto secret_value = co_await rc->execCommandCoro("get %s", local_constants::redis::secret_key_name.c_str());

        if (secret_value.isNil()) {
            // if failed to get (not exists)
            auto new_secret_value = core::token::generate_random_string(local_constants::redis::secret_key_length);
            // try to set
            auto new_secret_set_result = co_await rc->execCommandCoro("set %s %s ex %d nx", 
                local_constants::redis::secret_key_name.c_str(), 
                new_secret_value.c_str(), 
                local_constants::secret_key_expire_seconds
            );

            if (new_secret_set_result.isNil()) {
                // if failed to set (another server just set it)
                secret_value = co_await rc->execCommandCoro("get %s", local_constants::redis::secret_key_name.c_str());
            } else {
                // if set by this server
                co_return new_secret_value;
            }
        }
        co_return secret_value.asString();
    }
}