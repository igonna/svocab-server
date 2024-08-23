#include <drogon/HttpRequest.h>
#include <random>


namespace local_utils {

static inline std::string generate_random_string(size_t length) {
    std::random_device rd; 
    std::mt19937 gen(rd()); 
    std::uniform_int_distribution<> dis(32, 122); 
 
    std::string result; 
    result.reserve(length); 
 
    for (size_t i = 0; i < length; ++i) { 
        result += static_cast<char>(dis(gen)); 
    } 
 
    return result; 
}

namespace Error {

namespace Name {
    static constexpr char not_valid_header[] = "not_valid_headers";
};


namespace Response {
    static void MAKE_ERROR(std::function<void (const HttpResponsePtr &)> &callback, drogon::HttpStatusCode status_code, const char* message) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(status_code);
        resp->setBody(message);
        callback(resp);
    }

    static void NOT_VALID_HEADER(std::function<void (const HttpResponsePtr &)> &callback) {
        MAKE_ERROR(callback, k400BadRequest, Name::not_valid_header);
    }
}

}
}