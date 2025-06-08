#include "RevenueCat.h"
#include <drogon/HttpClient.h>
#include "utils/exceptions.h"
#include "utils/utils.h"
#include <trantor/utils/Date.h>
#include <drogon/drogon.h>

using namespace drogon;

void RevenueCat::initAndStart(const Json::Value &config) {
    url = config["url"].asString();
    std::tie(host, path) = local_utils::url::extract_host_and_path_from_url(url);
    api_key = config["api_key"].asString();
    project_id = config["project_id"].asString();
    webhook_key = config["webhook_key"].asString();

    if (webhook_key.empty()) throw std::runtime_error("webhook_key is empty");
}

drogon::Task<bool> RevenueCat::is_user_subscriber(const uint64_t user_id) try {
    auto users_db = app().getDbClient(local_constants::users_db);

    auto result = co_await users_db->execSqlCoro(
        "SELECT COALESCE(urs.user_id, nps.user_id) AS user_id,"
            " urs.expires_time,"
            " urs.last_verify_time,"
            " (nps.user_id IS NOT NULL) AS is_user_free_subscriber,"
            " (urs.expires_time >= now()) AS is_subscription_active,"
            " COALESCE(now() >= urs.expires_time + interval '1 hour', false) AS is_expired_more_than_1hr,"
            " COALESCE(now() >= urs.last_verify_time + interval '3 minutes', false) AS is_short_verify_delay_elapsed,"
            " COALESCE(now() >= urs.last_verify_time + interval '6 hours', false) AS is_long_verify_delay_elapsed"
        " FROM no_payment_subscribers nps"
        " FULL OUTER JOIN user_revenue_cat_subscription urs ON urs.user_id = nps.user_id"
        " WHERE COALESCE(urs.user_id, nps.user_id) = $1", user_id
    );

    if (result.empty()) co_return false;

    auto is_user_free_subscriber = result[0]["is_user_free_subscriber"].as<bool>();
    if (is_user_free_subscriber) co_return true;

    auto is_subscription_active = result[0]["is_subscription_active"].as<bool>();
    if (is_subscription_active) co_return true;

    auto is_expired_more_than_1hr = result[0]["is_expired_more_than_1hr"].as<bool>();
    auto is_short_verify_delay_elapsed = result[0]["is_short_verify_delay_elapsed"].as<bool>();
    auto is_long_verify_delay_elapsed = result[0]["is_long_verify_delay_elapsed"].as<bool>();

    bool can_verify_subscription = is_expired_more_than_1hr ? is_long_verify_delay_elapsed : is_short_verify_delay_elapsed;

    if (can_verify_subscription) {
        try {
            auto is_updated = co_await update_user_subscription_status(user_id, users_db);

            if (is_updated) {
                auto result_after_update = co_await users_db->execSqlCoro(
                    " SELECT user_id, (urs.expires_time >= now()) AS is_subscription_active"
                    " FROM user_revenue_cat_subscription WHERE user_id = $1", user_id
                );

                if (result_after_update.empty()) {
                    co_return false;
                } else {
                    is_subscription_active = result_after_update[0]["is_subscription_active"].as<bool>();
                    co_return is_subscription_active;
                }
            }
        } catch (...) {}
    }

    co_return !is_expired_more_than_1hr;
} catch (const std::exception& e) {
    LOG_ERROR << "is_user_subscriber exception: " << e.what();
    throw;
}

drogon::Task<bool> RevenueCat::update_user_subscription_status(const uint64_t user_id, drogon::orm::DbClientPtr users_db) {
    auto subscriber_entitlement = co_await get_revenuecat_subscriber_entitlement(std::to_string(user_id));

    if (subscriber_entitlement.has_value()) {
        const auto& e = subscriber_entitlement.value();

        auto expires_date_json = local_utils::json::get_checked(e, "expires_date");
        if (expires_date_json.isNull()) throw std::runtime_error("update_user_subscription_status exception: expires_date_json is null");

        auto expires_date = expires_date_json.as<std::string>();
        auto product_id = local_utils::json::get_checked(e, "product_identifier").as<std::string>();
        auto purchase_date = local_utils::json::get_checked(e, "purchase_date").as<std::string>();

        auto result = co_await users_db->execSqlCoro(
            " INSERT INTO user_revenue_cat_subscription (user_id, product_id, purchase_time, expires_time)"
            " VALUES ($1, $2, $3::timestamptz, $4::timestamptz) ON CONFLICT (user_id) DO UPDATE SET"
            " product_id = EXCLUDED.product_id, purchase_time = EXCLUDED.purchase_time, expires_time = EXCLUDED.expires_time, last_verify_time = NOW()",
            user_id, product_id, purchase_date, expires_date
        );

        co_return result.affectedRows();
    } else {
        auto result = co_await users_db->execSqlCoro("DELETE FROM user_revenue_cat_subscription WHERE user_id = $1", user_id);
        co_return result.affectedRows();
    }

    co_return false;
}

Task<std::optional<Json::Value>> RevenueCat::get_revenuecat_subscriber_entitlement(const std::string& user_app_id) try {
    auto client = HttpClient::newHttpClient(host);

    auto req = HttpRequest::newHttpRequest();
    req->setMethod(Get);
    req->setPath(path + '/' + user_app_id);
    req->addHeader("Authorization", api_key);

    auto result = co_await client->sendRequestCoro(req);

    auto status_code = result->getStatusCode();
    if (status_code == k200OK) {
        auto json = result->getJsonObject();
        if (!json) throw std::runtime_error("not a json");

        const auto& subscriber = local_utils::json::get_checked(*json, "subscriber");
        const auto& entitlements = local_utils::json::get_checked(subscriber, "entitlements");

        if (entitlements.empty()) co_return std::nullopt;
        else co_return *(entitlements.begin());
    } else if (status_code == k429TooManyRequests) {
        throw exception::too_many_requests_exception();
    } else {
        throw std::runtime_error("revenue cat wrong status code: " + std::to_string(status_code));
    }
} catch (const std::exception& e) {
    throw;
}