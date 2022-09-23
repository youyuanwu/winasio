#include "boost/asio.hpp"
#include "boost/winasio/named_pipe/named_pipe_protocol.hpp"
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace net = boost::asio;
namespace winnet = boost::winasio;

enum { max_length = 1024 };

int main(int argc, char *argv[]) {
  try {
    net::io_context io_context;

    winnet::named_pipe_protocol<net::io_context::executor_type>::endpoint ep(
        "\\\\.\\pipe\\mynamedpipe");
    winnet::named_pipe_protocol<net::io_context::executor_type>::pipe pipe(
        io_context);
    pipe.connect(ep);

    std::cout << "Enter message: ";
    char request[max_length];
    std::cin.getline(request, max_length);
    size_t request_length = std::strlen(request);
    net::write(pipe, net::buffer(request, request_length));

    char reply[max_length];
    size_t reply_length = net::read(pipe, net::buffer(reply, request_length));
    std::cout << "Reply is: ";
    std::cout.write(reply, reply_length);
    std::cout << "\n";
  } catch (std::exception &e) {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
