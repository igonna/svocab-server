#pragma once

#include <exception>

namespace exception {
    struct custom_exception : public std::exception {
        const char* what() const noexcept override {
            return "custom_exception";
        }
    };

    struct unknown_type : public custom_exception {
        const char* what() const noexcept override {
            return "unknown_type";
        }
    };

    struct not_valid_body : public custom_exception {
        const char* what() const noexcept override {
            return "not_valid_body";
        }
    };

    struct data_extract_fail : public custom_exception {
        const char* what() const noexcept override {
            return "data_extract_fail";
        }
    };
}