#pragma once
#include "boost/asio.hpp"
#include "boost/winasio/named_pipe/named_pipe_protocol.hpp"

namespace net = boost::asio;
namespace winnet = boost::winasio;

class session
  : public std::enable_shared_from_this<session>
{
public:
  session(winnet::named_pipe_protocol<net::io_context::executor_type>::server_pipe socket)
    : socket_(std::move(socket))
  {
    std::cout << "session constructed" << std::endl;
  }

  void start()
  {
    do_read();
  }

private:
  void do_read()
  {
    std::cout<<"do_read"<<std::endl;
    auto self(shared_from_this());
    socket_.async_read_some(net::buffer(data_, max_length),
        [this, self](std::error_code ec, std::size_t length)
        {
          std::cout<<"do_read handler"<<std::endl;
          if (!ec)
          {
            do_write(length);
          }else{
            std::cout<<"do_read handler error: "<< ec.message() <<std::endl;
          }
        });
  }

  void do_write(std::size_t length)
  {
    std::cout<<"do_write"<<std::endl;
    auto self(shared_from_this());
    boost::asio::async_write(socket_, net::buffer(data_, length),
        [this, self](std::error_code ec, std::size_t /*length*/)
        {
          std::cout<<"do_write handler"<<std::endl;
          if (!ec)
          {
            do_read();
          }
        });
  }

  winnet::named_pipe_protocol<net::io_context::executor_type>::server_pipe socket_;
  enum { max_length = 1024 };
  char data_[max_length];
};