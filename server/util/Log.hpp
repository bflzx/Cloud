#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <mutex>
#include <memory>
#include <format>
#include <chrono>
#include <thread>
#include <cerrno>
#include <cstring>
#include <atomic>
#include "SingletonBase.hpp"




//normal
#define LOG(lv, msg) \
    do{\
        Logger::getInstance().log(lv, msg, __FILE__, __LINE__); \
    }while(0)

#define LOG_DEBUG(msg) LOG(DEBUG, msg)
#define LOG_INFO(msg)  LOG(INFO,  msg)
#define LOG_WARN(msg)  LOG(WARN,  msg)
#define LOG_ERROR(msg) LOG(ERROR, msg)
#define LOG_FATAL(msg) LOG(FATAL, msg)

//Access
#define LOG_ACCESS(ip,port,method,url,device) \
    do{\
        Logger::getInstance().log_access(ip,port,method,url,device); \
    }while(0)

//Exception
#define LOG_EXC(lv,Exception) \
    do{\
         Logger::getInstance().log(lv, (Exception).what(), (Exception).getFile(), (Exception).getLine()); \
    }while(0)
#define LOG_DEBUG_EXC(Exception) LOG_EXC(DEBUG, Exception)
#define LOG_INFO_EXC(Exception)  LOG_EXC(INFO, Exception)
#define LOG_WARN_EXC(Exception)  LOG_EXC(WARN, Exception)
#define LOG_ERROR_EXC(Exception) LOG_EXC(ERROR, Exception)
#define LOG_FATAL_EXC(Exception) LOG_EXC(FATAL, Exception)


enum LogLevel
{
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FATAL
};

//[2026‑09‑01 22:10:30.123] [ERROR] [tid:12048] main.cpp:42 : open file failed
constexpr const char* LogFormat = "[{}] [{}] [tid:{}] {}:{} : {}\n";
const LogLevel DEFAULT_LEVEL = INFO;

// [2026-09-16 23:10:22] [127.0.0.1] [51324] [POST] [/api/login] [Mozilla/5.0 (Windows NT 10.0; Win64; x64)]
constexpr const char* AccessLogFormat = "[{}] [{}] [{}] [{}] [{}] [{}]\n";

class Logger : public SingletonBase<Logger> 
{
    friend class SingletonBase<Logger>;
public:
    void log(LogLevel level,const std::string& message,const char* filename,const int line)
    {
        if(level < log_level_) return;
        std::string time = getLocalTimeStr();
        std::lock_guard<std::mutex> lock(mutex_);
        std::string writeMessage;
        writeMessage = std::format(LogFormat,time,level2str(level),
        tidToString(std::this_thread::get_id()),cutFileName(filename),line,message);

        if(local_storage_ && out_file_.is_open())
        {
            out_file_ << writeMessage;
            if(out_file_.fail())
            {
                int last_error = errno;
                if(!io_error_output_)
                {
                    std::cerr << "Logger file IO error, switch to console output :" 
                    << std::strerror(last_error) << std::endl; 
                    io_error_output_ = true;
                }
                out_file_.clear();
                out_file_.close();
                local_storage_ = false;
            }
        }
        else
        {
            std::cout << writeMessage;
        }
    }
// [2026-09-16 23:10:22] [127.0.0.1] [51324] [POST] [/api/login] [Mozilla/5.0 (Windows NT 10.0; Win64; x64)]
    void log_access(const std::string& client_ip,uint16_t client_port,const std::string& client_method
                    ,const std::string& client_url,const std::string& device)
    {
        std::string time = getLocalTimeStr();
        std::lock_guard<std::mutex> lock(mutex_);

        std::string writeMessage;
        writeMessage = std::format(AccessLogFormat,time,client_ip,client_port,client_method,client_url
                                    ,device);
        if(access_open_ && access_out_file_.is_open())
        {
            access_out_file_ << writeMessage;
            if(access_out_file_.fail())
            {
                int last_error = errno;
                if(!access_io_error_output_)
                {
                    std::cerr << "Logger file IO error, switch to console output :" 
                    << std::strerror(last_error) << std::endl; 
                    access_io_error_output_ = true;
                }
                access_out_file_.clear();
                access_out_file_.close();
                access_open_ = false;
            }
        }
        else
        {
            std::cout << writeMessage;
        }
    }
private:
    std::string getLocalTimeStr()
    {
    auto now = std::chrono::system_clock::now();
    // 截断到秒
    auto sec_part = std::chrono::floor<std::chrono::seconds>(now);
    auto ms_part = std::chrono::duration_cast<std::chrono::milliseconds>(now - sec_part);
    return std::format("{:%Y-%m-%d %H:%M:%S}.{:03d}", sec_part, ms_part.count());
    }
    const char* level2str(LogLevel level)
    {
        if(level == DEBUG) return "DEBUG";
        else if(level == INFO) return "INFO";
        else if(level == WARN) return "WARN";
        else if(level == ERROR) return "ERROR";
        else return "FATAL";
    }
    const char* cutFileName(const char* filename)
    {
        if(!filename) return "";
        const int len = std::strlen(filename);
        for(int i = len-1; i >= 0; --i)
        {
            if(filename[i] == '/')
            {
                return filename + i + 1;
            }
        }
        return filename;
    }
    std::string tidToString(std::thread::id tid)
    {
        std::ostringstream oss;
        oss << tid;
        return oss.str();
    }
public:
    std::string getStoragePath() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(local_storage_)
        {
            return storage_path_;
        }
        return "";
    }
    bool getLocalStorage() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(local_storage_) return true;
        return false;
    }
    LogLevel getLogLevel() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return log_level_;
    }
public:
    bool openLocalStorage(const std::string& path)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        bool ok = setPath_no_lock(path);
        if(ok)
        {
            local_storage_ = true;
        }
        return ok;
    }
    bool openAccessLog(const std::string& access_path)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if(access_out_file_.is_open())
        {
            access_out_file_.close();
        }

        access_out_file_.open(access_path,std::ios::app);
        if(!access_out_file_.is_open())
        {
            return false;
        }

        access_open_ = true;
        access_path_ = access_path;
        return true;
    }
    void closeLocalStorage()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(out_file_.is_open())
        {
            out_file_.close();
        }
        local_storage_ = false;
    }
    bool setPath(const std::string& new_path)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return setPath_no_lock(new_path);
    }
    void setLevel(LogLevel Level)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        log_level_ = Level;
    }
public:
    bool init(const std::string& localPath,LogLevel level)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        storage_path_ = localPath;
        bool is = setPath_no_lock(localPath);
        if(!is)
        {
            return false;
        }
        local_storage_ = true;
        io_error_output_ = false;
        log_level_ = level;
        return true;
    }
    bool init(const std::string& localPath)
    {
        return init(localPath,DEFAULT_LEVEL);
    }
    bool init(LogLevel level)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        local_storage_ = false;
        storage_path_ = "";
        log_level_ = level;
        return true;
    }
    bool init()
    {
        return init(DEFAULT_LEVEL);
    }
private:
    bool setPath_no_lock(const std::string& new_path)
    {
        if(out_file_.is_open())
        {
            out_file_.close();
        }

        out_file_.open(new_path,std::ios::app);
        if(!out_file_.is_open())
        {
            return false;
        }
        storage_path_ = new_path;
        io_error_output_ = false;
        return true;
    }
private:
    Logger() = default;
    ~Logger()
    {
        if(out_file_.is_open())
        {
            out_file_.close();
        }
    }
private:
    bool local_storage_ = false;
    std::string storage_path_;
    std::atomic<LogLevel> log_level_ = DEFAULT_LEVEL;
    std::ofstream out_file_;
    mutable std::mutex mutex_;
    bool io_error_output_ = false;

    std::string access_path_;
    std::ofstream access_out_file_;
    bool access_open_ = false;
    bool access_io_error_output_ = false;
};