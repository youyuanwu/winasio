#pragma once

#include "boost/winasio/named_pipe/named_pipe_protocol.hpp"

namespace net = boost::asio;
namespace winnet = boost::winasio;

boost::system::error_code make_client_call(std::string const &msg,
                                           std::string &reply_ret) {
  boost::system::error_code ec;

  net::io_context io_context;
  winnet::named_pipe_protocol<net::io_context::executor_type>::endpoint ep(
      "\\\\.\\pipe\\mynamedpipe");
  winnet::named_pipe_protocol<net::io_context::executor_type>::pipe pipe(
      io_context);
  pipe.connect(ep, ec, 2000 /*2 sec timeout*/);

  if (ec.failed()) {
    return ec;
  }

  const int max_length = 1024;

  net::write(pipe, net::buffer(msg, msg.length()), ec);
  if (ec.failed()) {
    return ec;
  }

  char reply[max_length];
  size_t reply_length = net::read(pipe, net::buffer(reply, msg.length()), ec);
  DBG_UNREFERENCED_LOCAL_VARIABLE(reply_length);
  if (ec.failed()) {
    return ec;
  }
  std::string replyMsg = std::string(reply, msg.length());
  reply_ret = replyMsg;
  return ec;
}