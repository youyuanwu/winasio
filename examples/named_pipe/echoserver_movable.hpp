//
// Copyright (c) 2022 Youyuan Wu
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "boost/asio.hpp"
#include "boost/winasio/named_pipe/named_pipe_protocol.hpp"
#include "echoserver_session.hpp"
#include <cstdlib>
#include <iostream>
#include <memory>
#include <spdlog/spdlog.h>
#include <utility>

// Used as example and testing

class server_movable {
public:
  server_movable(
      net::io_context &io_context,
      winnet::named_pipe_protocol<net::io_context::executor_type>::endpoint ep)
      : acceptor_(io_context, ep) {
    do_accept();
  }

private:
  void do_accept() {
    spdlog::debug("do_accept");
    acceptor_.async_accept(
        [this](boost::system::error_code ec,
               winnet::named_pipe_protocol<net::io_context::executor_type>::pipe
                   socket) {
          if (!ec) {
            spdlog::debug("do_accept handler ok. making session");
            std::make_shared<session>(std::move(socket))->start();
          } else {
            spdlog::error("accept handler error: {}", ec.message());
          }

          do_accept();
        });
  }

  winnet::named_pipe_protocol<net::io_context::executor_type>::acceptor
      acceptor_;
};
