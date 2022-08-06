#pragma once

#include "boost/winasio/named_pipe/named_pipe_protocol.hpp"
#include "echoserver_session.hpp"

namespace net = boost::asio;

class server
{
public:
  server(net::io_context& io_context, winnet::named_pipe_protocol<net::io_context::executor_type>::endpoint ep)
    : acceptor_(io_context, ep),
      pipe_(io_context)
  {
    do_accept();
  }

private:
  void do_accept()
  {
    std::cout<<"do_accept"<<std::endl;
    acceptor_.async_accept(pipe_,
        [this](boost::system::error_code ec)
        {
          std::cout<<"do_accept handler"<<std::endl;
          if (!ec)
          {
            std::make_shared<session>(std::move(pipe_))->start();
          }

          do_accept();
        });
  }

  winnet::named_pipe_protocol<net::io_context::executor_type>::acceptor acceptor_;
  winnet::named_pipe_protocol<net::io_context::executor_type>::server_pipe pipe_;
};