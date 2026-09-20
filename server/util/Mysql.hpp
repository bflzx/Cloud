#pragma once

#include <iostream>
#include <string>
#include <mysql/mysql.h>
#include "Log.hpp"
#include "Exception.hpp"

class Mysql
{
public:
    bool exec(const std::string& sql)
    {
        if(!mysql_) return false;
        
        int ret = mysql_query(mysql_,sql.c_str());

        if(ret)
        {
            unsigned int eno = mysql_errno(mysql_);
            const char* err_txt = mysql_error(mysql_);
            THROW_EXC(DBException,db_err::MYSQL_QUERY,
                        std::string("mysql_query error(") + std::to_string(eno) + ") : " + err_txt + " SQL: " + sql);
        }
        return true;
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
        if(res == nullptr)
        {
            unsigned int eno = mysql_errno(mysql_);
            const char* err_txt = mysql_error(mysql_);
            if(eno)
            {
                THROW_EXC(DBException,db_err::MYSQL_STORE_RESULT,
                    std::string("mysql_store_result error(") + std::to_string(eno) + ") : " + err_txt);
            }
            return {};
        }

        int cols = mysql_num_fields(res);
        size_t rows = mysql_num_rows(res) + 1;
        std::vector<std::vector<std::string>> result(rows,std::vector<std::string>(cols));
        
        //1.第一行存储元信息
        MYSQL_FIELD* fields = mysql_fetch_fields(res);
        for(int j = 0;j < cols;j++)
        {
            result[0][j] = fields[j].name;
        }
        //2.其余存储查询结构
        for(size_t i = 1;i < rows;i++)
        {
            MYSQL_ROW row = mysql_fetch_row(res);
            if(!row) break;
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
            THROW_EXC(DBException,db_err::MYSQL_CONSTRUCTOR,"Mysql Constructor faield!");
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
