#include "echoserver.hpp"
#include "boost/asio.hpp"
#include "boost/winasio/named_pipe/named_pipe_protocol.hpp"
#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>

namespace net = boost::asio;

int main(int argc, char *argv[]) {
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
