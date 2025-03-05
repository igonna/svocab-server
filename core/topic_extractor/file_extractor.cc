#include "topic_extractor_factory.h"
#include "utils/utils.h"
#include "utils/exceptions.h"
#include "core/text_extraction/html_extractor.h"
#include "core/text_processing/text_processor.h"
#include "core/website_fetch/website_fetch.h"
#include "plugins/FrequentWords/FrequentWords.h"
#include "core/text_extraction/pdf_extractor.h"
#include "core/text_extraction/html_extractor.h"
#include "core/postgres/queries.h"

namespace core::topic_extractor {

using namespace drogon;

drogon::Task<> FileExtractor::init(drogon::HttpRequestPtr req) {
    MultiPartParser multi_part_parser;
    if (multi_part_parser.parse(req) != 0) {
        throw exception::not_valid_body();
    }

    auto &files = multi_part_parser.getFiles();

    if (files.empty()) {
        throw exception::not_valid_body();
    }

    auto file = files[0];
    auto content_type = file.getContentType();

    try {
        auto users_db = app().getDbClient(local_constants::users_db);
        auto user_id = local_utils::attributes::get_value<uint64_t>(req, local_constants::attributes::user_id);

        auto user_has_subscription = co_await core::sql::does_user_have_subscription(users_db, user_id);
        auto [limit_reached, limit_resets_time] = co_await core::sql::user_limits::is_processing_files_limit_reached(
            users_db, user_id, user_has_subscription
        );

        if (limit_reached) {
            throw exception::file_processing_limit_reached(std::to_string(limit_resets_time));
        }

        std::string text;

        if (content_type == drogon::ContentType::CT_TEXT_PLAIN) {
            text = file.fileContent();
        } else if (content_type == drogon::ContentType::CT_APPLICATION_PDF) {
            text = core::text_extraction::get_text_from_pdf(file.fileData(), file.fileLength());
        } else if (content_type == drogon::ContentType::CT_TEXT_HTML) {
            text = file.fileContent();
            std::tie(title, text) = core::text_extraction::extract_text_and_title_from_html(text);
        } else {
            throw exception::not_valid_body();
        }

        auto src_lang = req->getHeader("srcLang");
        auto dst_lang = req->getHeader("dstLang");

        auto frequent_words_plugin = app().getPlugin<FrequentWords>();
        auto prioritized_words_sequence = core::text_processing::create_prioritized_words_sequence(text, frequent_words_plugin, src_lang.c_str(), dst_lang.c_str());

        if (prioritized_words_sequence.empty()) throw exception::data_extract_fail();

        if (title.empty()) {
            title = local_utils::strings::get_title_from_str(text, local_constants::title_max_length);
        } else {
            title = local_utils::strings::get_title_from_str(title, local_constants::title_max_length);
        }
        origin = file.getFileName();
        origin_type = "file";
        words_sequence = std::move(prioritized_words_sequence);

        co_await core::sql::user_limits::increase_processing_files_count(users_db, user_id, 1);

    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
        throw;
    }

    co_return;
}

}