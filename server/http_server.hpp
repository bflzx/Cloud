#include <iostream>
#include <fstream>
#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/beast.hpp>


namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;

using tcp = asio::ip::tcp;
using asio::awaitable;
using asio::co_spawn;
using asio::use_awaitable;
using asio::detached;

class HttpServer
{
public:
    HttpServer(asio::io_context& ioc,uint16_t port)
    :ioc_(ioc),port_(port)
    {}
public:
    void start()
    {
        co_spawn(ioc_.get_executor(),accepotr_handle(),detached);
    }
private:
    std::string read_file(std::string&& path)
    {
        std::ifstream in(path);

        std::stringstream ss;
        ss << in.rdbuf();

        return ss.str();
    }
    std::string get_mime(const std::string& path)
    {
        if(path.ends_with(".js")) return "application/javascript";
        if(path.ends_with(".css")) return "text/css";
        if(path.ends_with(".svg")) return "image/svg+xml";

        return "text/html;charset=utf-8";
    }
    awaitable<void> router_handle(http::request<http::string_body>& req,http::response<http::string_body>& res)
    {
        res.result(http::status::internal_server_error);

        std::string target = req.target();
        std::string body;
        std::string mime;

        if(target.starts_with("/assets/") || target == "/favicon.svg" || target == "/icons.svg")
        {
            body = read_file("../web/dist" + target);
            mime = get_mime(target);
        }
        else if(target == "/api/login")
        {
            
        }
        else
        {
            body = read_file("../web/dist/index.html");
            mime = get_mime(target);
        }

        res.result(http::status::ok);
        res.set(http::field::content_type,mime);
        res.body() = std::move(body);
        res.prepare_payload();
        co_return;
    }

    awaitable<void> client_handle(tcp::socket sock)
    {
        beast::tcp_stream stream(std::move(sock));
        beast::flat_buffer buff;
        beast::error_code ec;

        for(;;)
        {
            http::request<http::string_body> req;
            co_await http::async_read(stream,buff,req,asio::redirect_error(use_awaitable,ec));
            
            std::cout << req.target() << std::endl;
            http::response<http::string_body> res;
            co_await router_handle(req,res);

            if(!req.keep_alive())
            {
                res.keep_alive(false);
            }


            co_await http::async_write(stream,res,asio::redirect_error(use_awaitable,ec));

            buff.clear();
        }
    }
    awaitable<void> accepotr_handle()
    {
        beast::error_code ec;
        tcp::acceptor acceptor(ioc_);
        acceptor.open(tcp::v4(),ec);
        acceptor.set_option(tcp::acceptor::reuse_address(true),ec);
        acceptor.bind(tcp::endpoint(tcp::v4(),port_),ec);
        acceptor.listen(asio::socket_base::max_listen_connections,ec);
        
        for(;;)
        {
            tcp::socket sock = co_await acceptor.async_accept(asio::redirect_error(use_awaitable,ec));
            co_spawn(acceptor.get_executor(),client_handle(std::move(sock)),detached);
        }
    }
private:
    asio::io_context& ioc_;
    uint16_t port_;
};