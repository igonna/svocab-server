#pragma once
#include <drogon/plugins/Plugin.h>
#include <drogon/utils/coroutine.h>
#include <drogon/orm/DbClient.h>

class AppleId : public drogon::Plugin<AppleId> {
public:
	AppleId() {};
	void initAndStart(const Json::Value &config) override;
	void shutdown() override {};

    // user_id, email
    drogon::Task<std::tuple<std::string, std::optional<std::string>>> get_email_from_apple_id_token(const std::string& apple_id_token);

private:
    std::string apple_bundle_id;
};