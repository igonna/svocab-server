#include <drogon/drogon.h>
#include "controllers/test_ctrl.hpp"

int main() {

    // drogon::app().registerController(std::make_shared<MyController>());
    // drogon::app().run();

    drogon::app().loadConfigFile("../config.json")
        .setThreadNum(16)
        .run();
    return 0;
}