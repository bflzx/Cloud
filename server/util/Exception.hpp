#pragma once
#include <exception>
#include <string>
#include "util/Log.hpp"

#define THROW_EXC(ExceptionType,errcode,errMsg) \
    throw ExceptionType(errcode,errMsg,__FILE__,__LINE__);

class BaseException : public std::exception
{
public:
    BaseException(int code,std::string msg,const char* file,int line)
    :code_(code),msg_(msg),file_(file),line_(line)
    {}
    const char* what() const noexcept override
    {
        return msg_.c_str();
    }

    int getCode() const noexcept
    {
        return code_;
    }

    const char* getFile() const noexcept
    {
        return file_;
    }

    int getLine() const noexcept
    {
        return line_;
    }
protected:
    int code_;
    std::string msg_;
    const char* file_;
    int line_;
};

class NetworkException : public BaseException
{
public:
    using BaseException::BaseException;
};

class DBException : public BaseException
{
public:
    using BaseException::BaseException;
};

class JsonException : public BaseException
{
public:
    using BaseException::BaseException;
};

class SaltException : public BaseException
{
public:
    using BaseException::BaseException;
};

class BusinessException : public BaseException
{
public:
    using BaseException::BaseException;
};