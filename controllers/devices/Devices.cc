#include "Devices.h"
#include "utils/utils.h"
#include "utils/constants.h"
#include "core/secret/secret_key.h"
#include "core/token/token.h"
#include "core/device/revoke.h"
#include "core/devices_info/devices_info.h"

namespace v1
{
Task<> Devices::getBareToken(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    auto refresh_token = req->getHeader("refreshToken");
    auto device_os_version = req->getHeader("deviceOsVersion");
    auto device_model = req->getHeader("deviceModel");
    auto device_name = req->getHeader("deviceName");

    if (refresh_token.empty() || device_os_version.empty() || device_model.empty()) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    uint64_t user_id;
    uint32_t device_num;
    std::string device_refresh_token;
    
    try {
        std::tie(user_id, device_num, device_refresh_token) = core::token::decode_device_refresh_token(refresh_token);
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::ERROR(callback, k401Unauthorized, "");
    }

    auto ip_addr = local_utils::bytes::swap_endian(req->peerAddr().ipNetEndian());

    auto time_now = local_utils::time::get_unix_timestamp();
    if (device_name.empty()) {
        device_name = co_await core::devices_info::get_device_name_by_model(device_model);
    }

    // check if valid device_refresh_token and update
    try {
        auto users_db = app().getDbClient(local_constants::users_db);

        auto result = co_await users_db->execSqlCoro("update user_devices set device_os_version = $1, device_model = $2, device_name = $3, "
            "token_last_request_time = $4, last_ip_address = $5 where user_id = $6 and device_refresh_token is not NULL and device_num = $7 and device_refresh_token = $8", 
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

        result = co_await users_db->execSqlCoro("update users set token_last_request_time = $1 where id = $2", 
            time_now, 
            user_id
        );

        if (result.affectedRows() == 0) {
            throw std::runtime_error("token_last_request_time_no_updated");
        }

        auto session_result = co_await users_db->execSqlCoro(
            "insert into user_sessions (user_id, started_at, finished_at, ip_address) values ($1, $2, $3, $4) returning session_id", user_id, time_now, time_now, ip_addr
        );

        if (session_result.empty()) {
            throw std::runtime_error("failed_to_create_session");
        }

        auto session_id = session_result[0]["session_id"].as<uint64_t>();

        // get secret key
        auto rc = app().getRedisClient();
        auto secret_key = co_await core::secret::get_secret_key(rc);
        auto bare_token = core::token::create_device_bare_token(user_id, device_num, session_id, secret_key);
        
        co_return local_utils::Response::OK(callback, bare_token);
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
    }
    co_return local_utils::Response::Error::SERVER_ERROR(callback);
}

Task<> Devices::logOutOfAllDevices(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    try {
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);
        auto current_device_num = local_utils::attributes::get_value<uint32_t>(req, local_constants::attributes::device_num);

        auto users_db = app().getDbClient(local_constants::users_db);

        auto devices_result = co_await users_db->execSqlCoro(
            "select device_num from user_devices where user_id = $1 and device_num != $2", user_id, current_device_num
        );

        auto rc = app().getRedisClient();
        for (const auto &row : devices_result) {
            auto cur_device_num = row["device_num"].as<uint32_t>();
            co_await core::device::revoke_device(rc, user_id, cur_device_num);
        }

        // revoke all user_devices
        auto resultDevices = co_await users_db->execSqlCoro(
            "update user_devices set device_refresh_token = NULL where user_id = $1 and device_num != $2", user_id, current_device_num
        );

        co_return local_utils::Response::OK(callback, "");
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
    }
    co_return local_utils::Response::Error::SERVER_ERROR(callback);
}
};