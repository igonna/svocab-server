#include <drogon/drogon.h>

using namespace drogon;

int main() {
    drogon::app().setCustomErrorHandler([](drogon::HttpStatusCode code) -> drogon::HttpResponsePtr {
        auto resp = drogon::HttpResponse::newHttpResponse();
        if (code == drogon::k404NotFound) {
            resp->setStatusCode(drogon::k410Gone);
        } else {
            resp->setStatusCode(code);
        }
        return resp;
    });

    drogon::app().loadConfigFile("./config.json").run();
    return 0;
}