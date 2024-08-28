#pragma once

#include <drogon/HttpController.h>

using namespace drogon;

class UserDeleteAccount : public drogon::HttpController<UserDeleteAccount>
{
  public:
    
    METHOD_LIST_BEGIN
    // list path definitions here;
    // PATH_ADD("/path", "filter1", "filter2", HttpMethod1, HttpMethod2...);
    METHOD_LIST_END
};
