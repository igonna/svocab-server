#pragma once

#include <drogon/HttpController.h>
#include <drogon/utils/coroutine.h>

using namespace drogon;

namespace v1
{
class GlobalTopics : public drogon::HttpController<GlobalTopics>
{
public:
};
}