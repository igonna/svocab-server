#include "subscription.h"
#include "utils/utils.h"
#include "utils/exceptions.h"
#include "plugins/RevenueCat/RevenueCat.h"

namespace v1
{
Task<bool> is_user_blocked_from_verification(drogon::nosql::RedisClientPtr redis_client, uint64_t user_id) {
    const auto key = local_utils::redis::create_block_verify_subscription_key(user_id);
    auto exists = co_await redis_client->execCommandCoro(
        "exists %s",
        key.c_str()
    );

    co_return std::stoi(exists.asString()) > 0;
}

Task<> block_user_from_verification(drogon::nosql::RedisClientPtr redis_client, uint64_t user_id) {
    const auto key = local_utils::redis::create_block_verify_subscription_key(user_id);
    co_await redis_client->execCommandCoro("set %s %s ex %d nx",
        key.c_str(),
        "",
        local_constants::user_blocked_subscription_update_seconds
    );
    co_return;
}

Task<> Subscription::update(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) try {
    auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

    auto rc = app().getRedisClient();
    auto is_blocked = co_await is_user_blocked_from_verification(rc, user_id);
    if (is_blocked) throw exception::too_many_requests_exception();

    auto revenue_cat_plugin = app().getPlugin<RevenueCat>();

    auto is_user_subscriber = co_await revenue_cat_plugin->is_user_subscriber(user_id);
    if (is_user_subscriber) co_return local_utils::Response::OK(callback, "");

    auto users_db = app().getDbClient(local_constants::users_db);
    auto is_updated = co_await revenue_cat_plugin->update_user_subscription_status(user_id, users_db);

    co_await block_user_from_verification(rc, user_id);
    if (is_updated) {
        co_return local_utils::Response::OK(callback, "");
    } else {
        co_return local_utils::Response::Error::ERROR(callback, k304NotModified, "");
    }
} catch (const exception::too_many_requests_exception& e) {
    LOG_ERROR << e.what();
    co_return local_utils::Response::Error::ERROR(callback, k429TooManyRequests, "");
} catch (const std::exception& e) {
    LOG_ERROR << e.what();
    co_return local_utils::Response::Error::SERVER_ERROR(callback);
}
}