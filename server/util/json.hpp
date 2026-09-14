#pragma once
#include <jsoncpp/json/json.h>
#include <string>
#include <memory>
#include <fstream>
#include "Log.hpp"
#include "Exception.hpp"

namespace json
{
    std::string serialize(const Json::Value& root)
    {
        Json::StreamWriterBuilder swb;
        std::unique_ptr<Json::StreamWriter> sw(swb.newStreamWriter());

        std::stringstream ss;
        int ret = sw->write(root,&ss);
        if(ret != 0)
        {
            LOG_WARN("Serialize Failed!");
            return {};
        }
        
        return ss.str();
    }

    Json::Value unserialize(const std::string& str)
    {
        Json::CharReaderBuilder crb;
        std::unique_ptr<Json::CharReader> cr(crb.newCharReader());
        
        Json::Value root;
        bool ret = cr->parse(str.c_str(),str.c_str() + str.size(),&root,nullptr);
        if(!ret)
        {
            LOG_WARN("Unserialize failed!");
            return {};
        }
        return root;
    }
}