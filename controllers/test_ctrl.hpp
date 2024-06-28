#include <drogon/HttpSimpleController.h>
#include <drogon/orm/DbClient.h>
#include <drogon/drogon.h>

using namespace drogon;

class TestCtrl:public drogon::HttpSimpleController<TestCtrl>
{
public:
    void asyncHandleHttpRequest(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback) override;
    PATH_LIST_BEGIN
    PATH_ADD("/test", Post);
    PATH_LIST_END
};