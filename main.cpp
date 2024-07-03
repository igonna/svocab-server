#include <drogon/drogon.h>

int main() {

    // drogon::app().registerController(std::make_shared<MyController>());
    // drogon::app().run();

    drogon::app().loadConfigFile("../config.json")
        .setThreadNum(16)
        .run();
    return 0;
}