#pragma once

#include <exception>

namespace exception {
    struct unknown_type : public std::exception {
        const char* what() const noexcept override {
            return "unknown_type";
        }
    };
    
    struct not_valid_body : public std::exception {
        const char* what() const noexcept override {
            return "not_valid_body";
        }
    };
    
    struct data_extract_fail : public std::exception {
        const char* what() const noexcept override {
            return "data_extract_fail";
        }
    };
    }