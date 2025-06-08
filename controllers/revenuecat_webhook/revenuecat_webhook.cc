#include "revenuecat_webhook.h"
#include "utils/utils.h"
#include "plugins/RevenueCat/RevenueCat.h"

namespace v1
{
Task<> RevenueCatWebhook::revenueCatWebhook(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) try {
    auto revenue_cat_plugin = app().getPlugin<RevenueCat>();
    if (req->getHeader("Authorization") != revenue_cat_plugin->get_webhook_key()) {
        throw std::runtime_error("not valid webhook key");
    }

    auto json = req->getJsonObject();
    if (!json) throw std::runtime_error("not a json");

    const auto& event = local_utils::json::get_checked(*json, "event");
    auto app_user_id_json = local_utils::json::get_checked(event, "app_user_id");

    if (!app_user_id_json.isNumeric()) throw std::runtime_error("id is not numeric");
    auto app_user_id = app_user_id_json.as<uint64_t>();

    auto users_db = app().getDbClient(local_constants::users_db);
    std::ignore = co_await revenue_cat_plugin->update_user_subscription_status(app_user_id, users_db);

    co_return local_utils::Response::OK(callback, "");
} catch (const std::exception& e) {
    LOG_ERROR << "revenueCatWebhook: " << e.what();
    co_return local_utils::Response::Error::SERVER_ERROR(callback);
}
}