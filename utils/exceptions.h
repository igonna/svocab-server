#pragma once

#include <exception>
#include <string>

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

    struct not_valid_header : public custom_exception {
        const char* what() const noexcept override {
            return "not_valid_header";
        }
    };

    struct data_extract_fail : public custom_exception {
        const char* what() const noexcept override {
            return "data_extract_fail";
        }
    };

    struct file_processing_limit_reached : public custom_exception {
        std::string message;
    
        file_processing_limit_reached(const std::string& msg) : message(msg) {}
    
        const char* what() const noexcept override {
            return message.c_str();
        }
    };
}