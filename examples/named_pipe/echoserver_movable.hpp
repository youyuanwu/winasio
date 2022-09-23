#include "boost/asio.hpp"
#include "boost/winasio/named_pipe/named_pipe_protocol.hpp"
#include "echoserver_session.hpp"
#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>

// Used as example and testing

class server_movable
{
public:
    server_movable(
        net::io_context & io_context,
        winnet::named_pipe_protocol<net::io_context::executor_type>::endpoint ep)
        : acceptor_(io_context, ep)
    {
        do_accept();
    }

private:
    void do_accept()
    {
        std::cout << "do_accept" << std::endl;
        acceptor_.async_accept(
            [this](boost::system::error_code ec, winnet::named_pipe_protocol<net::io_context::executor_type>::pipe socket)
            {
                if (!ec)
                {
                    std::cout << "do_accept handler ok. making session" << std::endl;
                    std::make_shared<session>(std::move(socket))->start();
                }
                else
                {
                    std::cout << "accept handler error: " << ec.message() << std::endl;
                }

                do_accept();
            });
    }

    winnet::named_pipe_protocol<net::io_context::executor_type>::acceptor
        acceptor_;
};
