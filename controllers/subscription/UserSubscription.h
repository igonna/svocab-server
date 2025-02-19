#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

namespace v1
{
class UserSubscription : public drogon::HttpController<UserSubscription>
{
public:
	Task<> addSubscription(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
    Task<> updateSubscriptionStatus(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);
    Task<> isSubscriptionCancelled(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);

	METHOD_LIST_BEGIN
	METHOD_ADD(UserSubscription::addSubscription, "/add_subscription", Post);
    METHOD_ADD(UserSubscription::updateSubscriptionStatus, "/update_subscription_status", Post);
    METHOD_ADD(UserSubscription::isSubscriptionCancelled, "/is_subscription_cancelled", Get);
	METHOD_LIST_END
};
}