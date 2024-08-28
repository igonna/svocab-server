#include "UserDevices.h"
#include "utils/utils.h"
#include "utils/constants.h"

namespace v1
{
Task<> UserDevices::getBareToken(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    auto refresh_token = req->getHeader("refreshToken");
    auto device_os_version = req->getHeader("deviceOsVersion");
    auto device_model = req->getHeader("deviceModel");

    uint32_t user_id;
    uint32_t device_num;
    std::string device_refresh_token;
    
    try {
        std::tie(user_id, device_num, device_refresh_token) = local_utils::token::decode_device_refresh_token(refresh_token);
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::ERROR(callback, k401Unauthorized, "");
    }

    auto ip_addr = req->peerAddr().ipNetEndian();
    auto time_now = local_utils::time::get_unix_timestamp();
    std::string device_name = "";

    // check if valid device_refresh_token and update
    try {
        auto user_db = app().getDbClient(local_constants::users_db);

        auto result = co_await user_db->execSqlCoro("update user_devices set device_os_version = $1, device_model = $2, device_name = $3, "
            "token_last_request_time = $4, last_ip_address = $5 where user_id = $6 and device_num = $7 and device_refresh_token = $8", 
            device_os_version,
            device_model,
            device_name,
            time_now,
            ip_addr,
            user_id,
            device_num,
            device_refresh_token
        );

        if (result.affectedRows() == 0) {
            co_return local_utils::Response::Error::ERROR(callback, k401Unauthorized, "");
        }

    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    // get secret key
    auto rc = app().getRedisClient();
    auto secret_key = co_await local_utils::secret_key::get_secret_key(rc);
    auto bare_token = local_utils::token::create_device_bare_token(user_id, device_num, secret_key);
    
    co_return local_utils::Response::OK(callback, bare_token);
}

Task<> getDevices(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    co_return;
}

Task<> revokeDevice(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    co_return;
}

};