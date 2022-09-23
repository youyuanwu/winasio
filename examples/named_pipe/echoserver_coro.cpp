#include "boost/winasio/named_pipe/named_pipe_protocol.hpp"
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/write.hpp>
#include <cstdio>

namespace net = boost::asio;
namespace winnet = boost::winasio;

using net::awaitable;
using net::co_spawn;
using net::detached;
using net::use_awaitable;
namespace this_coro = net::this_coro;

#if defined(ASIO_ENABLE_HANDLER_TRACKING)
#define use_awaitable \
    net::use_awaitable_t(__FILE__, __LINE__, __PRETTY_FUNCTION__)
#endif

net::awaitable<void>
echo(winnet::named_pipe_protocol<net::any_io_executor>::pipe socket)
{
    std::cout << "echo invoked" << std::endl;
    try
    {
        char data[1024];
        for (;;)
        {
            std::size_t n =
                co_await socket.async_read_some(net::buffer(data), use_awaitable);
            co_await async_write(socket, net::buffer(data, n), use_awaitable);
        }
    }
    catch (std::exception & e)
    {
        std::printf("echo Exception: %s\n", e.what());
    }
}

awaitable<void> listener()
{
    auto executor = co_await this_coro::executor;
    // tcp::acceptor acceptor(executor, {tcp::v4(), 55555});
    winnet::named_pipe_protocol<net::any_io_executor>::endpoint ep(
        "\\\\.\\pipe\\mynamedpipe");
    winnet::named_pipe_protocol<net::any_io_executor>::acceptor acceptor(executor, ep);
    for (;;)
    {
        winnet::named_pipe_protocol<net::any_io_executor>::pipe socket =
            co_await acceptor.async_accept(use_awaitable);
        std::cout << "listener spawing" << std::endl;
        co_spawn(executor, echo(std::move(socket)), detached);
    }
}

int main()
{
    try
    {
        net::io_context io_context(1);

        net::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](auto, auto)
                           { io_context.stop(); });

        co_spawn(io_context, listener(), detached);

        io_context.run();
    }
    catch (std::exception & e)
    {
        std::printf("Exception: %s\n", e.what());
    }
}
