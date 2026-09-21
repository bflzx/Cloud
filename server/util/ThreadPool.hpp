#pragma once

#include <iostream>
#include <thread>
#include <queue>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <functional>
#include "SingletonBase.hpp"
#include "Log.hpp"


using func = std::function<void()>;

class ThreadPool : public SingletonBase<ThreadPool>
{
    friend class SingletonBase<ThreadPool>;
    static const int default_capacity = 4;

public:
    void push(const func& f)
    {
            std::lock_guard<std::mutex> lock(mutex_);
            if(stopped_)
            {
                LOG_WARN("ThreadPool has stopped, cannot push task!");
                return;
            }
            task_queue_.push(f);
            cv_.notify_one();
    }

    void start(int thread_num)
    {
        if(thread_num <= 0)
        {
            LOG_WARN("thread_num is invalid!");
            return;
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if(started_)
            {
                LOG_WARN("ThreadPool has started!");
                return;
            }
            started_ = true;
        }
        pool_capacity_ = thread_num;
        for(int i = 0; i < thread_num; i++)
        {
            std::thread t(std::bind(&ThreadPool::threadTask,this));
            thread_pool_.push_back(std::move(t));
        }
    }

    void start()
    {
        start(default_capacity);
    }

    void stop()
    {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if(stopped_) return;
            stopped_ = true;
        }
        cv_.notify_all();
        for(auto& t : thread_pool_)
        {
            if(t.joinable())
            {
                t.join();
            }
        }
    }

private:
    void threadTask()
    {
        for(;;)
        {
            func task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                while(!stopped_ && task_queue_.empty())
                {
                    cv_.wait(lock);
                }
                if(stopped_ && task_queue_.empty()) return;
                task = std::move(task_queue_.front());
                task_queue_.pop();
            }
            if(task) task();
        }
    }

private:
    ThreadPool() = default;
    ~ThreadPool()
    {
        stop();
    }

private:
    int pool_capacity_ = default_capacity;
    std::queue<func> task_queue_;
    std::vector<std::thread> thread_pool_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stopped_ = false;
    bool started_ = false;
};