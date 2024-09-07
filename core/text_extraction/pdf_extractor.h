#pragma once

#include <string>

namespace core::text_extraction {
    std::string get_text_from_pdf(const char* file_data, size_t file_length);
}