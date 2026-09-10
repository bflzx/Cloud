#include "http_server.hpp"
#include "Log.hpp"
int main()
{
    uint16_t port = 8080;
    boost::asio::io_context ioc;

    Logger::getInstance().init("./log/log.txt");

    HttpServer server(ioc,port);
    std::cout << "请访问http://192.168.195.129:8080" << std::endl;
    server.start();
    ioc.run();
    return 0;
}