#pragma once

#include <iostream>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <atomic>
#include "Mysql.hpp"
#include "Log.hpp"
#include "SingletonBase.hpp"


class MysqlPool : public SingletonBase<MysqlPool>
{
    friend class SingletonBase<MysqlPool>;
    static const int default_capacity = 4;

public:
    std::unique_ptr<Mysql> borrow()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while(!stopped_ && pool_.empty())
        {
            cv_.wait(lock);
        }
        if(stopped_ && pool_.empty()) return nullptr;
        std::unique_ptr<Mysql> conn = std::move(pool_.front());
        pool_.pop();
        lock.unlock();

        if(conn->ping() || conn->reconnect()) return conn;
        return nullptr;
    }

    void give_back(std::unique_ptr<Mysql> conn)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        
        pool_.push(std::move(conn));
        cv_.notify_one();
    }

    bool all_returned()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return pool_.size() == pool_capacity_;
    }

public:
    void start(const std::string& host,const std::string& username,
        const std::string& password,const int port,const std::string& database,const int capacity = default_capacity)
    {
        host_ = host;
        username_ = username;
        password_ =password;
        port_ = port;
        database_ = database;
        pool_capacity_ = capacity;
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if(started_)
            {
                LOG_WARN("MysqlPool has started_!");
                return;
            }
            started_ = true;
        }
        for (int i = 0; i < capacity; ++i)
        {
            std::unique_ptr<Mysql> conn = std::make_unique<Mysql>(host_,username_,password_,port_,database_);
            if(conn->get_mysql())
            {
                pool_.push(std::move(conn));
            }
            else
            {
                LOG_WARN("one mysql connect create fiald!");
                pool_capacity_--;
            }
        }
    }
    void stop()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if(pool_.size() != pool_capacity_)
        {
            LOG_WARN("Some connect have not return!");
        }
        stopped_ = true;
        cv_.notify_all();
        
        while(!pool_.empty())
        {
            std::unique_ptr<Mysql> conn = std::move(pool_.front());
            pool_.pop();
        }
    }
private:
    MysqlPool() = default;
    ~MysqlPool()
    {
        stop();
    }
private:
    std::string host_;
    std::string username_;
    std::string password_;
    std::string database_;
    int port_;

    int pool_capacity_;
    std::queue<std::unique_ptr<Mysql>> pool_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> stopped_ = false;
    std::atomic<bool> started_ = false;
};