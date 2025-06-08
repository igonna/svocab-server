#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

namespace v1
{
class RevenueCatWebhook : public drogon::HttpController<RevenueCatWebhook>
{
public:
    Task<> revenueCatWebhook(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback);

    METHOD_LIST_BEGIN
    METHOD_ADD(RevenueCatWebhook::revenueCatWebhook, "/post", Post);
    METHOD_LIST_END
};
}