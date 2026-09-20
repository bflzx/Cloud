#include <string>
#include "http_server.hpp"
#include "util.hpp"

const std::string host = "192.168.195.129";
const std::string username = "root";
const std::string password = "lixiaoxu1880";
const int port = 3306;
const std::string database = "Cloud";


int main()
{
    try
    {
        uint16_t http_port = 8080;
        boost::asio::io_context ioc;

        Logger::getInstance().init("./log/log.txt");
        MysqlPool::getInstance().start(host,username,password,port,database);


        HttpServer server(ioc,http_port);
        std::cout << "请访问http://192.168.195.129:8080" << std::endl;
        server.start();
        ioc.run();
    }
    catch(DBException& e)
    {
        LOG_FATAL_EXC(e);
        return 1;
    }
    return 0;
}