#include "UserDeleteAccount.h"
#include "utils/constants.h"
#include "utils/utils.h"

namespace v1
{
Task<> UserDeleteAccount::deleteAccount(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    auto users_db = app().getDbClient(local_constants::users_db);
    auto refresh_token = req->getHeader("refreshToken");

    try {
        auto user_id = local_utils::attributes::get_value<uint32_t>(req, local_constants::attributes::user_id);
        auto device_num = local_utils::attributes::get_value<uint32_t>(req, local_constants::attributes::device_num);
        auto [user_id_1, device_num_1, device_refresh_token] = local_utils::token::decode_device_refresh_token(refresh_token);

        if (user_id != user_id_1 || device_num != device_num_1) {
            throw std::runtime_error("no_match");
        }

        // check if valid device_token
        auto result = co_await users_db->execSqlCoro("select 1 from user_devices where user_id = $1 "
            "and device_num = $2 and device_refresh_token = $3 limit 1",
            user_id,
            device_num,
            device_refresh_token
        );

        if (!result.size()) {
            co_return local_utils::Response::Error::FORBIDDEN(callback, "not_valid_device_token");
        }

        // revoke all user_devices
        auto resultDevices = co_await users_db->execSqlCoro("select device_num from user_devices where user_id = $1", user_id);

        auto rc = app().getRedisClient();
        for (const auto &row : resultDevices) {
            auto cur_device_num = row["device_num"].as<uint32_t>();
            co_await local_utils::redis::revoke::revoke_device(rc, user_id, cur_device_num);
        }

        // delete account
        auto deleteResult = co_await users_db->execSqlCoro("delete from users where id = $1", user_id);
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return callback(HttpResponse::newHttpResponse());
}
}