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

class MysqlPool;

class MysqlConnGuard
{
public:
    MysqlConnGuard(std::unique_ptr<Mysql> conn,MysqlPool* pool)
        :conn_(std::move(conn)),pool_(pool)
    {}

    MysqlConnGuard(const MysqlConnGuard&) = delete;
    MysqlConnGuard& operator=(const MysqlConnGuard&) = delete;

    MysqlConnGuard(MysqlConnGuard&& other) noexcept;
    MysqlConnGuard& operator=(MysqlConnGuard&& other);
    ~MysqlConnGuard();

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

class MysqlPool : public SingletonBase<MysqlPool>
{
    friend class SingletonBase<MysqlPool>;
    static const int default_capacity = 4;

public:
    MysqlConnGuard borrow()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while(!stopped_ && pool_.empty())
        {
            cv_.wait(lock);
        }
        if(stopped_ && pool_.empty()) return MysqlConnGuard{nullptr,nullptr};
        std::unique_ptr<Mysql> conn = std::move(pool_.front());
        pool_.pop();
        lock.unlock();

        if(!conn->ping())
        {
            if(conn->reconnect())
            {
                return MysqlConnGuard(std::move(conn),this);
            }
            else
            {
                auto conn = std::move(create_conn());
                if(conn->ping()) return MysqlConnGuard(std::move(conn),this);
                
            }
        }

        return MysqlConnGuard(std::move(conn),this);
    }

    std::unique_ptr<Mysql> create_conn()
    {
        std::unique_ptr<Mysql> conn = std::make_unique<Mysql>(host_,username_,password_,port_,database_);
        std::unique_lock<std::mutex> lock(mutex_);
        if(conn->ping()) return std::move(conn);

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

inline MysqlConnGuard::MysqlConnGuard(MysqlConnGuard&& other) noexcept
    :conn_(std::move(other.conn_)),pool_(other.pool_)
    {
        other.pool_ = nullptr;
    }

inline MysqlConnGuard& MysqlConnGuard::operator=(MysqlConnGuard&& other)
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
    
inline MysqlConnGuard::~MysqlConnGuard()
    {
        if(conn_ && pool_)
        {
            pool_->give_back(std::move(conn_));
        }
    }