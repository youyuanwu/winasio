#include "echoserver_movable.hpp"

int main(int argc, char *argv[]) {
  try {
    // if (argc != 2)
    // {
    //   std::cerr << "Usage: async_tcp_echo_server <port>\n";
    //   return 1;
    // }

    net::io_context io_context;

    winnet::named_pipe_protocol<net::io_context::executor_type>::endpoint ep(
        "\\\\.\\pipe\\mynamedpipe");

    server_movable s(io_context, ep);

    io_context.run();
  } catch (std::exception &e) {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
