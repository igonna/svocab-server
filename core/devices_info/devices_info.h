#pragma once

#include <drogon/utils/coroutine.h>
#include <drogon/drogon.h>

namespace core::devices_info {
    drogon::Task<std::string> get_device_name_by_model(const std::string& model);
}