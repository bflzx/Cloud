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

namespace api
{
    
}