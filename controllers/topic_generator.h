#include <drogon/HttpSimpleController.h>
#include <drogon/orm/DbClient.h>
#include <drogon/drogon.h>

using namespace drogon;

class TopicGenerator:public drogon::HttpSimpleController<TopicGenerator>
{
public:
    void asyncHandleHttpRequest(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback) override;

    METHOD_LIST_BEGIN
    PATH_ADD("/gen_topic", Post);
    METHOD_LIST_END
};