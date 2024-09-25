// #include <drogon/orm/DbClient.h>

#include "devices_info.h"

#include <unicode/uchar.h>
#include <unicode/unistr.h>
#include <unicode/ucnv.h>

#include "utils/constants.h"
#include "utils/utils.h"

namespace core::devices_info {
    icu::UnicodeString convert_UTF16LE_to_UnicodeString(const std::string& utf16le_str) {
        UErrorCode error = U_ZERO_ERROR;

        // Open a converter for UTF-16LE
        UConverter* utf16le_converter = ucnv_open("UTF-16LE", &error);
        if (U_FAILURE(error)) {
            throw std::runtime_error("Failed to open UTF-16LE converter");
        }

        // Convert the input UTF-16LE string to a UTF-16 UnicodeString
        int32_t utf16_len = ucnv_toUChars(utf16le_converter, nullptr, 0, utf16le_str.c_str(), utf16le_str.length(), &error);
        if (error != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(error)) {
            ucnv_close(utf16le_converter);
            throw std::runtime_error("Failed to calculate buffer size for UTF-16");
        }

        // Allocate a buffer for the UTF-16 string
        std::u16string utf16Str(utf16_len, '\0');
        error = U_ZERO_ERROR;
        ucnv_toUChars(utf16le_converter, reinterpret_cast<UChar*>(&utf16Str[0]), utf16_len, utf16le_str.c_str(), utf16le_str.length(), &error);
        if (U_FAILURE(error)) {
            ucnv_close(utf16le_converter);
            throw std::runtime_error("Failed to convert UTF-16LE to UTF-16");
        }

        // Close the UTF-16LE converter
        ucnv_close(utf16le_converter);

        // Convert the UTF-16 string to UTF-8 using ICU's UnicodeString class
        icu::UnicodeString unicode_str(utf16Str.c_str(), utf16_len);
        return unicode_str;
    }

    // model, marketing_name
    drogon::Task<std::unordered_map<std::string, std::string>> fetch_all_devices_models_and_marketing_names() {
        auto& supported_devices_config = drogon::app().getCustomConfig()["supported_devices"];

        icu::UnicodeString csv_devices;
        {
            auto host = supported_devices_config["host"].as<std::string>();
            auto path = supported_devices_config["path"].as<std::string>();

            auto client = drogon::HttpClient::newHttpClient(host);
            auto request = drogon::HttpRequest::newHttpRequest();
            request->setMethod(drogon::Get);
            request->setPath(path);

            auto response = co_await client->sendRequestCoro(request);

            if (response->statusCode() != drogon::k200OK) {
                throw std::runtime_error("can_not_fetch");
            }

            std::string body = std::string(response->getBody());
            csv_devices = convert_UTF16LE_to_UnicodeString(body);
        }

        uint32_t size = csv_devices.length();
        uint32_t comma_num = 0;

        std::unordered_map<std::string, std::string> model_marketing;

        icu::UnicodeString current_marketing_name;
        icu::UnicodeString current_model;

        const UChar comma_char = ',';
        const UChar whitespace_char = ' ';
        const UChar new_line_char = '\n';
        const UChar carriage_return_char = '\r';
        char prev_char = 0;
        for (uint32_t i = 0; i < size; ++i) {
            if (csv_devices[i] == carriage_return_char)
                continue;
            if (prev_char == whitespace_char && csv_devices[i] == whitespace_char) {
                continue;
            } if ((csv_devices[i] == new_line_char || i + 1 == size) && comma_num) {
                if (current_marketing_name.length() && current_model.length()) {
                    std::string current_marketing_name_utf8;
                    std::string current_model_utf8;

                    current_marketing_name.toUTF8String(current_marketing_name_utf8);
                    current_model.toUTF8String(current_model_utf8);

                    model_marketing.insert({std::move(current_model_utf8), std::move(current_marketing_name_utf8)});
                }

                current_marketing_name.remove();
                current_model.remove();

                comma_num = 0;
                continue;
            } else if (csv_devices[i] == comma_char) {
                ++comma_num;
                continue;
            } else if (comma_num == 1) {
                current_marketing_name += csv_devices[i];
            } else if (comma_num == 3) {
                current_model += csv_devices[i];
            }
        }

        co_return model_marketing;
    }

    drogon::Task<std::string> get_device_name_by_model(const std::string& model) {
        try {
            auto devices_info_db = drogon::app().getDbClient(local_constants::devices_info_db);
            auto result = co_await devices_info_db->execSqlCoro("select marketing_name from devices_info where model = $1 limit 1", model);

            if (result.size()) {
                co_return result[0]["marketing_name"].as<std::string>();
            }
        } catch (const std::exception& e) {
            LOG_ERROR << e.what();
            co_return "";
        }

        drogon::async_run([]() -> drogon::Task<void> {
            try {
                auto rc = drogon::app().getRedisClient();
                // try to set
                auto device_info_update_result = co_await rc->execCommandCoro("set %s %s ex %d nx", 
                    local_constants::redis::devices_info_update_key_name.c_str(), 
                    "", 
                    local_constants::devices_info_update_key_expire_seconds
                );

                // key already exists
                if (device_info_update_result.isNil()) {
                    co_return;
                }

                auto devices_models_and_marketing_names = co_await fetch_all_devices_models_and_marketing_names();

                std::string values;
                for (auto& model_and_marketing_name : devices_models_and_marketing_names) {
                    values += "($$" + model_and_marketing_name.first + "$$," + "$$" + model_and_marketing_name.second + "$$),";
                }
                if (values.size()) {
                    values.pop_back();
                }

                auto devices_info_db = drogon::app().getDbClient(local_constants::devices_info_db);
                
                std::string query = "insert into devices_info (model, marketing_name) values " + values + " on conflict (model) do nothing";
                
                co_await devices_info_db->execSqlCoro(query);
            } catch (const std::exception& e) {
                LOG_ERROR << e.what();
            }
            co_return;
        });

        co_return "";
    }
}