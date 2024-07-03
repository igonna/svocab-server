#include "topic_generator.hpp"
#include <drogon/HttpRequest.h>
#include <poppler/cpp/poppler-document.h>
#include <poppler/cpp/poppler-page.h>
#include <gumbo.h>
#include <unicode/uchar.h>
#include <unicode/unistr.h>
#include <unicode/brkiter.h>
#include <memory>


// Function to extract text from a GumboNode
void extractText(GumboNode *node, std::string &text)
{
    if (node->type == GUMBO_NODE_TEXT)
    {
        text.append(node->v.text.text);
    }
    else if (node->type == GUMBO_NODE_ELEMENT && node->v.element.tag != GUMBO_TAG_SCRIPT && node->v.element.tag != GUMBO_TAG_STYLE)
    {
        GumboVector *children = &node->v.element.children;
        for (unsigned int i = 0; i < children->length; ++i)
        {
            extractText(static_cast<GumboNode *>(children->data[i]), text);
        }
        if (node->v.element.tag == GUMBO_TAG_P || node->v.element.tag == GUMBO_TAG_BR)
        {
            text.append("\n");
        }
    }
}


std::vector<char> get_bytes_from_file(const char* file_data, size_t file_length) {
    std::unique_ptr<poppler::document> doc{ poppler::document::load_from_raw_data(file_data, file_length) };
    const auto pages_count = doc->pages();

    std::vector<char> bytes;
    for (int i = 0; i < pages_count; ++i) {
        std::unique_ptr<poppler::page> page{ doc->create_page(i) };
        if (page) {
            auto page_bytes = page->text().to_utf8();
            bytes.insert(bytes.end(), page_bytes.begin(), page_bytes.end());
        }
    }

    return bytes;
}

std::optional<Json::Value> parse_json(const std::string& data) {
    Json::Value root;
    Json::Reader reader;
    bool success = reader.parse(data, root);

    // std::array<const char*, 9> required_members = {
    //     "topic_title",
    //     "src_lang",
    //     "dst_langs",
    //     "result_type",
    //     "merge_type",
    //     "process_weight_limit",
    //     "reposiotry",
    //     "texts",
    //     "urls"
    // };

    // root.isMember("");

    if (success)
        return root;
    else
        return std::nullopt;
}

void ERROR_RESPONSE(std::function<void (const HttpResponsePtr &)> &callback, drogon::HttpStatusCode status_code, const char* message) {
    auto resp = HttpResponse::newHttpResponse();
    resp->setBody(message);
    resp->setStatusCode(status_code);
    callback(resp);
}

enum class ResultType {create, update, preview};
enum class WordCountType {frequency, sum};

std::unordered_map<std::string, double_t> get_words_from_unicode_string_with_spaces(const icu::UnicodeString& unicode_string) {
    std::unordered_map<std::string, double_t> words_count;

    int32_t total_len = unicode_string.length();
    UChar32 c;
    bool is_alpha;
    icu::UnicodeString current_word;

    for (int32_t i = 0; i < total_len; ++i) {
        c = unicode_string.char32At(i);
        is_alpha = u_isalpha(c);
        current_word += u_tolower(c);
    }
    return words_count;
}


std::optional<std::unordered_map<std::string, double_t>> extract_words_from_text(icu::UnicodeString text, const char* language) {
    std::unordered_map<std::string, double_t> words_count;


    UErrorCode status = U_ZERO_ERROR;
    std::unique_ptr<icu::BreakIterator> bi(icu::BreakIterator::createWordInstance(icu::Locale(language), status));
    if (U_FAILURE(status)) {
        return std::nullopt;
    }

    // Set the text to be analyzed
    bi->setText(text);

    // Iterate through the words
    int32_t start = bi->first();
    for (int32_t end = bi->next(); end != icu::BreakIterator::DONE; start = end, end = bi->next()) {
        if (!u_isalpha(text[start]))
            continue;

        icu::UnicodeString word;
        text.extractBetween(start, end, word);
        word = word.toLower();

        std::string utf8Word;
        word.toUTF8String(utf8Word);

        words_count[std::move(utf8Word)] += 1.0f;
    }

    return words_count;
}

void TopicGenerator::asyncHandleHttpRequest(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback)
{
    //2.23 seconds
    // request verifying
    MultiPartParser multiPartParser;
    if (multiPartParser.parse(req) != 0)
        return ERROR_RESPONSE(callback, k400BadRequest, "body not valid");

    static const std::string metadata_parameter = "metadata";
    auto metadata = multiPartParser.getParameter<std::string>(metadata_parameter);

    if (metadata.size() == 0)
        return ERROR_RESPONSE(callback, k400BadRequest, "metadata is empty");

    // meta
    Json::Value json_meta;
    auto metadata_parsed = parse_json(metadata);
    if (metadata_parsed.has_value())
        json_meta = metadata_parsed.value();
    else
        return ERROR_RESPONSE(callback, k400BadRequest, "metadata parse error");

    auto texts = json_meta["texts"];
    auto urls = json_meta["urls"];
    auto &files = multiPartParser.getFiles();

    if (texts.size() == 0 && urls.size() == 0 && files.size() == 0)
        return ERROR_RESPONSE(callback, k400BadRequest, "no content to parse");

    // process files
    auto src_lang = json_meta["src_lang"].asString();
    std::vector<std::unordered_map<std::string, double_t>> files_words;
    files_words.reserve(files.size());

    for (auto& file : files) {
        std::vector file_bytes = get_bytes_from_file(file.fileData(), file.fileLength());
        icu::UnicodeString unicode_string = icu::UnicodeString::fromUTF8(icu::StringPiece(file_bytes.data()));
        file_bytes.clear();

        auto extracted_words = extract_words_from_text(unicode_string, src_lang.c_str());

        if (!extracted_words.has_value()) {
            std::string error_message = std::string("could not parse one of files: ") + file.getFileName();
            return ERROR_RESPONSE(callback, k400BadRequest, error_message.c_str());
        }

        files_words.push_back(std::move(extracted_words.value()));
        // files_words
    }

    // files_words.push_back(get_words_from_unicode_string_with_spaces(unicode_string));
    // get words from unicode_string
    // put words into files_words
    // 

    std::unordered_map<std::string, double_t> result_map;

    for (const auto& file_words : files_words) {
        for (const auto& pair : file_words) {
            result_map[pair.first] += pair.second;
        }
    }

    // 1 count words
    // 2 sum counts  or measure 

    // for (auto& i : files_words) {

    // }

    std::string output_result;

    for (const auto& pair : result_map) {
        output_result += pair.first + ": " + std::to_string(pair.second) + '\n';
    }

    auto resp = HttpResponse::newHttpResponse();
    resp->setBody(output_result);


    LOG_INFO << "The uploaded file has been saved to the ./uploads "
                "directory";
    callback(resp);

}
