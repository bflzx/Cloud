#pragma once
#include <exception>
#include <string>
#include "Log.hpp"


/*
NetworkException:0-999
DBException:1000-1999
JsonException:2000-2999
CryptoException:3000-3999
BusinessException:4000-4999
*/
namespace net_err
{
    enum err_code
    {
        
    };
}

namespace db_err
{
    enum err_code
    {
        DB_ERR = 1000,
        UNKNOWN = 1001,
        MYSQL_CONSTRUCTOR = 1002,
        MYSQL_QUERY = 1003,
        MYSQL_STORE_RESULT = 1004,
    };
}

namespace json_err
{
    enum err_code
    {

    };
}

namespace crypto_err
{
    enum err_code
    {
        CRYPTO_ERR = 3000,
        UNKNOWN = 3001,
        RAND_BYTES = 3002,
        PKCS5_PBKDF2_HMAC = 3003,
        JWT_VERIfY = 3004,
    };
}

namespace business_err
{
    enum err_code
    {
        BUSINESS_ERR = 4000,
        
    };
}


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

class CryptoException : public BaseException
{
public:
    using BaseException::BaseException;
};

class BusinessException : public BaseException
{
public:
    using BaseException::BaseException;
};
