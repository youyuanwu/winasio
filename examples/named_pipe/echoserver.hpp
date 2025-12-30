//
// Copyright (c) 2022 Youyuan Wu
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

#include "boost/winasio/named_pipe/named_pipe_protocol.hpp"
#include "echoserver_session.hpp"
#include <spdlog/spdlog.h>

namespace net = boost::asio;

class server {
public:
  server(
      net::io_context &io_context,
      winnet::named_pipe_protocol<net::io_context::executor_type>::endpoint ep)
      : acceptor_(io_context, ep), pipe_(io_context) {
    do_accept();
  }

private:
  void do_accept() {
    spdlog::debug("do_accept");
    acceptor_.async_accept(pipe_, [this](boost::system::error_code ec) {
      spdlog::debug("do_accept handler");
      if (!ec) {
        std::make_shared<session>(std::move(pipe_))->start();
      }

      do_accept();
    });
  }

  winnet::named_pipe_protocol<net::io_context::executor_type>::acceptor
      acceptor_;
  winnet::named_pipe_protocol<net::io_context::executor_type>::pipe pipe_;
};
