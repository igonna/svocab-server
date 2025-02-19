#include "UserDevices.h"
#include "utils/utils.h"
#include "utils/constants.h"
#include "core/secret/secret_key.h"
#include "core/token/token.h"
#include "core/device/revoke.h"
#include "core/devices_info/devices_info.h"

namespace v1
{
Task<> UserDevices::getBareToken(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
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

    auto ip_addr = req->peerAddr().ipNetEndian();
    auto time_now = local_utils::time::get_unix_timestamp();
    if (device_name.empty()) {
        device_name = co_await core::devices_info::get_device_name_by_model(device_model);
    }

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

        result = co_await user_db->execSqlCoro("update users set token_last_request_time = $1 where id = $2", 
            time_now, 
            user_id
        );

        if (result.affectedRows() == 0) {
            throw std::runtime_error("token_last_request_time_no_updated");
        }
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    // get secret key
    auto rc = app().getRedisClient();
    auto secret_key = co_await core::secret::get_secret_key(rc);
    auto bare_token = core::token::create_device_bare_token(user_id, device_num, secret_key);
    
    co_return local_utils::Response::OK(callback, bare_token);
}

Task<> UserDevices::getDevices(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    uint64_t user_id;
    uint32_t device_num;

    Json::Value response;

    try {
        user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);
        device_num = local_utils::attributes::get_value<uint32_t>(req, local_constants::attributes::device_num);

        auto users_db = app().getDbClient(local_constants::users_db);

        auto resultEmail = co_await users_db->execSqlCoro("select 1 from users where id = $1 and email is not null limit 1",
            user_id
        );

        bool is_email_linked = resultEmail.size();

        // revoke all user_devices
        auto resultDevices = co_await users_db->execSqlCoro("select device_num, device_name, "
            "device_os_version from user_devices where user_id = $1", 
            user_id
        );

        for (const auto &row : resultDevices) {
            auto cur_device_num = row["device_num"].as<uint32_t>();
            auto cur_device_name = row["device_name"].as<std::string>();
            auto cur_device_os_version = row["device_os_version"].as<std::string>();

            Json::Value cur_device;
            cur_device["device_num"] = cur_device_num;
            cur_device["device_name"] = cur_device_name;
            cur_device["device_os_version"] = cur_device_os_version;

            if (device_num == cur_device_num) {
                response["this_device"] = cur_device;
            } else {
                response["other_devices"].append(cur_device);
            }
        }

        response["email_link_required"] = !is_email_linked;
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return callback(HttpResponse::newHttpJsonResponse(response));
}

Task<> UserDevices::revokeDevice(HttpRequestPtr req, std::function<void(const HttpResponsePtr &)> callback) {
    auto device_num_to_delete_str = req->getHeader("deviceNum");

    if (!device_num_to_delete_str.size()) {
        co_return local_utils::Response::Error::NOT_VALID_HEADER(callback);
    }

    uint32_t device_num_to_delete = std::stoi(device_num_to_delete_str);

    try {
        uint64_t user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        auto users_db = app().getDbClient(local_constants::users_db);
        // delete device
        auto deleteResult = co_await users_db->execSqlCoro("delete from user_devices "
            "where user_id = $1 and device_num = $2",
            user_id,
            device_num_to_delete
        );

        if (!deleteResult.affectedRows()) {
            throw std::runtime_error("device_not_exists");
        }

        auto rc = app().getRedisClient();
        co_await core::device::revoke_device(rc, user_id, device_num_to_delete);
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        co_return local_utils::Response::Error::SOMETHING_WRONG(callback);
    }

    co_return callback(HttpResponse::newHttpResponse());
}

};