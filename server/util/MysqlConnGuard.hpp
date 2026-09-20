#pragma once

#include "MysqlPool.hpp"


class MysqlConnGuard
{
public:
    MysqlConnGuard(std::unique_ptr<Mysql> conn,MysqlPool* pool)
        :conn_(std::move(conn)),pool_(pool)
    {}

    MysqlConnGuard(const MysqlConnGuard&) = delete;
    MysqlConnGuard& operator=(const MysqlConnGuard&) = delete;

    MysqlConnGuard(MysqlConnGuard&& other) noexcept
    :conn_(std::move(other.conn_)),pool_(other.pool_)
    {
        other.pool_ = nullptr;
    }

    MysqlConnGuard& operator=(MysqlConnGuard&& other)
    {
        if(this != &other)
        {
            if(conn_ && pool_)
            {
                pool_->give_back(std::move(conn_));
            }
            conn_ = std::move(other.conn_);
            pool_ = other.pool_;
            other.pool_ = nullptr;
        }
        return *this;
    }
    
    ~MysqlConnGuard()
    {
        if(conn_ && pool_)
        {
            pool_->give_back(std::move(conn_));
        }
    }

    Mysql* get()
    {
        return conn_.get();
    }

    Mysql* operator->()
    {
        return conn_.get();
    }
private:
    std::unique_ptr<Mysql> conn_;
    MysqlPool* pool_;
};