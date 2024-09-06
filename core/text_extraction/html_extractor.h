#pragma once
#include <tuple>
#include <string>

namespace core::text_extraction
{
std::tuple<std::string, std::string> extract_text_and_title_from_html(const std::string& html);
}