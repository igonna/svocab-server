#include "pdf_extractor.h"
#include <poppler/cpp/poppler-document.h>
#include <poppler/cpp/poppler-page.h>

namespace core::text_extraction {
std::string get_text_from_pdf(const char* file_data, size_t file_length) {
    std::unique_ptr<poppler::document> doc{ poppler::document::load_from_raw_data(file_data, file_length) };
    const auto pages_count = doc->pages();

    std::string text;
    for (int i = 0; i < pages_count; ++i) {
        std::unique_ptr<poppler::page> page{ doc->create_page(i) };
        if (page) {
            auto page_bytes = page->text().to_utf8();
            text.insert(text.end(), page_bytes.begin(), page_bytes.end());
        }
    }
    return text;
}
}