#pragma once
#include <string>
#include "../util/json.hpp"
#include "../util/Exception.hpp"

namespace resp
{
    inline Json::Value ok(const std::string message = "ok",const Json::Value& data = Json::Value())
    {
        Json::Value r;
        r["code"] = business_err::OK;
        r["message"] = message;
        r["data"] = data;
        return r;
    }

    inline Json::Value fail(int code,const std::string& message,const Json::Value& data = Json::Value())
    {
        Json::Value r;
        r["code"] = code;
        r["message"] = message;
        r["data"] = data;
        return r;
    }
};