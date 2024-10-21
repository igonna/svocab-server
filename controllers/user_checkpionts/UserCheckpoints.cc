#include "UserCheckpoints.h"
#include "utils/constants.h"
#include "utils/utils.h"

namespace v1
{
Task<> UserCheckpoints::getCheckpoints(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    auto db = app().getDbClient(local_constants::users_db);

    Json::Value response;
    try {
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        auto result = co_await db->execSqlCoro("select principles_tutorial_passed, "
            "flow_tutorial_passed, filter_tutorial_passed from user_checkpoints "
            "where user_id = $1", user_id
        );

        if (result.empty()) {
            throw std::runtime_error("not_exists");
        }

        auto checkpoints = result[0];

        for (auto& checkpoint : checkpoints) {
            response[checkpoint.name()] = checkpoint.as<bool>();
        }
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return callback(HttpResponse::newHttpJsonResponse(response));
}

Task<> UserCheckpoints::setCheckpoint(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback)
{
    auto checkpoint_name = req->getHeader("checkpointName");
    auto value = req->getHeader("value");

    if (checkpoint_name.empty() || value.empty()) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        auto query = "update user_checkpoints set " + checkpoint_name + " = $1 where user_id = $2";

        auto result = co_await users_db->execSqlCoro(query, value == "true" ? true : false, user_id);

        if (!result.affectedRows()) {
            throw std::runtime_error("not_updated");
        }
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return local_utils::Response::OK(callback, "");
}
}