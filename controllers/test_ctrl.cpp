#include "test_ctrl.hpp"
#include <drogon/HttpRequest.h>

void TestCtrl::asyncHandleHttpRequest(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)> &&callback)
{
    MultiPartParser fileUpload;


    if (fileUpload.parse(req) != 0 || fileUpload.getFiles().size() < 1)
    {
        auto resp = HttpResponse::newHttpResponse();
        resp->setBody("Must be at least 1 file");
        resp->setStatusCode(k403Forbidden);
        callback(resp);
        return;
    }

    std::cout << "files count: " << fileUpload.getFiles().size() << std::endl;


    auto &files = fileUpload.getFiles();

    std::string result_md5;
    for (auto& file : files) {
        // file.fileData();
        // file.fileLength();
        auto md5 = file.getMd5();
        result_md5 += md5 + ", ";
        file.save();
    }


    auto resp = HttpResponse::newHttpResponse();
    resp->setBody("The server has calculated the files MD5 hashes " + result_md5);


    LOG_INFO << "The uploaded file has been saved to the ./uploads "
                "directory";
    callback(resp);





            
    // auto files = req->getBody();
    // //write your application logic here
    // auto resp = HttpResponse::newHttpResponse();
    // resp->setBody("<p>Hello, world!</p>");
    // resp->setExpiredTime(0);
    // callback(resp);
}
