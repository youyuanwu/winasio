//
// Copyright (c) 2022 Youyuan Wu
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "echoserver.hpp"
#include "boost/asio.hpp"
#include "boost/winasio/named_pipe/named_pipe_protocol.hpp"
#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>

namespace net = boost::asio;

int main() {
  try {

    net::io_context io_context;

    winnet::named_pipe_protocol<net::io_context::executor_type>::endpoint ep(
        "\\\\.\\pipe\\mynamedpipe");

    server s(io_context, ep);

    io_context.run();
  } catch (std::exception &e) {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
