#pragma once
#include <drogon/plugins/Plugin.h>
#include <drogon/utils/coroutine.h>
#include <drogon/orm/DbClient.h>

class RevenueCat : public drogon::Plugin<RevenueCat> {
public:
	RevenueCat() {};
	void initAndStart(const Json::Value &config) override;
	void shutdown() override {};


    drogon::Task<bool> is_user_subscriber(const uint64_t user_id);
    drogon::Task<bool> update_user_subscription_status(const uint64_t user_id, drogon::orm::DbClientPtr users_db);

    const std::string& get_webhook_key() const { return webhook_key; }

private:
    drogon::Task<std::optional<Json::Value>> get_revenuecat_subscriber_entitlement(const std::string& user_app_id);

private:
    std::string url;
    std::string host;
    std::string path;
    std::string api_key;
    std::string project_id;
    std::string webhook_key;
};