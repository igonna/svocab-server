#include "UserAccountInfo.h"
#include "utils/constants.h"
#include "utils/utils.h"

namespace v1
{
Task<> UserAccountInfo::getAccountInfo(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    auto db = app().getDbClient(local_constants::users_db);

    std::string email;
    bool has_subscription;

    try {
        auto user_id = local_utils::attributes::get_value<uint32_t>(req, local_constants::attributes::user_id);

        auto result = co_await db->execSqlCoro("SELECT u.id, u.email, CASE WHEN us.user_id IS NOT NULL THEN TRUE ELSE FALSE END "
            "AS has_subscription FROM users u LEFT JOIN user_google_play_subscription us ON u.id = us.user_id WHERE u.id = $1",
            user_id
        );

        email = result[0]["email"].as<std::string>();
        has_subscription = result[0]["has_subscription"].as<bool>();
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    Json::Value response;
    response["email"] = email;
    response["email_type"] = local_utils::email::get_email_type(email);
    response["has_subsciption"] = has_subscription;
    co_return callback(HttpResponse::newHttpJsonResponse(response));
}
}