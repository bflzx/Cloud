#pragma once

#include <iostream>
#include <string>
#include <mysql/mysql.h>
#include "Log.hpp"

class Mysql
{
public:
    bool exec(const std::string& sql)
    {
        if(!mysql_) return false;
        return mysql_query(mysql_,sql.c_str()) == 0;
    }
    bool ping()
    {
        return mysql_ && mysql_ping(mysql_) == 0;
    }
    bool reconnect()
    {
        return connect();
    }
    std::vector<std::vector<std::string>> get_result()
    {
        MYSQL_RES* res = mysql_store_result(mysql_);
        if(!res) return {};
        int cols = mysql_num_fields(res);
        int rows = mysql_num_rows(res) + 1;
        std::vector<std::vector<std::string>> result(rows,std::vector<std::string>(cols));
        
        //1.第一行存储元信息
        MYSQL_FIELD* fields = mysql_fetch_fields(res);
        for(int j = 0;j < cols;j++)
        {
            result[0][j] = fields[j].name;
        }
        //2.其余存储查询结构
        for(int i = 1;i < rows;i++)
        {
            MYSQL_ROW row = mysql_fetch_row(res);
            for(int j = 0;j < cols;j++)
            {
                result[i][j] = row[j] ? row[j] : "";
            }
        }

        mysql_free_result(res);
        return result;
    }

    void close()
    {
        if(mysql_) mysql_close(mysql_);
    }
public:
    Mysql(const std::string& host,const std::string& username,const std::string& password,const int port,const std::string& database)
    :host_(host),username_(username),password_(password),port_(port),database_(database)
    {
        if(!connect())
        {
            mysql_ = nullptr;
        }
    }

    ~Mysql()
    {
        if(mysql_) mysql_close(mysql_);
    }
    Mysql(const Mysql&) = delete;
    Mysql& operator=(const Mysql&) = delete;
public:
    MYSQL* get_mysql()
    {
        return mysql_;
    }
private:
    bool connect()
    {
        if(mysql_) mysql_close(mysql_);
        mysql_ = mysql_init(nullptr);
        if(mysql_ == nullptr)
        {
            LOG_ERROR("Mysql init failed!");
            return false;;
        }
        if(!mysql_real_connect(mysql_,host_.c_str(),username_.c_str(),
        password_.c_str(),database_.c_str(),port_,nullptr,0))
        {
            mysql_close(mysql_);
            mysql_ = nullptr;
            return false;
        }
        if(mysql_set_character_set(mysql_,"utf8mb4"))
        {
            mysql_close(mysql_);
            mysql_ = nullptr;
            return false;
        }

        return true;
    }
private:
    std::string host_;
    std::string username_;
    std::string password_;
    std::string database_;
    int port_;
    MYSQL* mysql_ = nullptr;
};
