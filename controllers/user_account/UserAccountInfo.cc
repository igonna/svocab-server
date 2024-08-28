#include "UserAccountInfo.h"

namespace v1
{
Task<> UserAccountInfo::getAccountInfo(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {


    Json::Value response;
    response["email"] = "email";

    // auto transaction = co_await app().getDbClient(local_constants::users_db)->newTransactionCoro();

    auto resp = HttpResponse::newHttpJsonResponse(response);
    co_return callback(HttpResponse::newHttpJsonResponse(response));
}
}