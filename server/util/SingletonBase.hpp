#pragma once

template<typename T>
class SingletonBase
{
public:
    static T& getInstance()
    {
        static T instance;
        return instance;
    }
    
    SingletonBase(const SingletonBase&) = delete;
    SingletonBase& operator=(const SingletonBase&) = delete;
    SingletonBase(SingletonBase&&) = delete;
    SingletonBase& operator=(SingletonBase&&) = delete;
protected:
    SingletonBase() = default;
    virtual ~SingletonBase() = default;
};