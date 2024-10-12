#include "UserSubscription.h"
#include "utils/utils.h"

namespace v1
{
Task<> UserSubscription::addSubscription(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    co_return local_utils::Response::Error::ERROR(callback, k501NotImplemented, "");
}

Task<> UserSubscription::updateSubscriptionStatus(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    co_return local_utils::Response::Error::ERROR(callback, k501NotImplemented, "");
}

Task<> UserSubscription::isSubscriptionCancelled(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    co_return local_utils::Response::Error::ERROR(callback, k501NotImplemented, "");
}

}