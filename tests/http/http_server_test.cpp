#include "boost/winasio/http/temp.hpp"
#include "gtest/gtest.h"

#include "beast_client.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>

#include <cstdio>
#include <utility>

namespace beast = boost::beast; // from <boost/beast.hpp>
namespace http = beast::http;   // from <boost/beast/http.hpp>
namespace net = boost::asio;    // from <boost/asio.hpp>
using tcp = net::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

namespace logging = boost::log;

void log_init() {
  logging::core::get()->set_filter(logging::trivial::severity >=
                                   logging::trivial::debug);
}

TEST(HTTPServer, server) {
  log_init();
  // init http module
  winnet::http::http_initializer init;
  // add https then this becomes https server
  std::wstring url = L"http://localhost:12356/winhttpapitest/";

  boost::system::error_code ec;
  net::io_context io_context;

  // open queue handle
  winnet::http::basic_http_handle<net::io_context::executor_type> queue(
      io_context);
  queue.assign(winnet::http::open_raw_http_queue());
  winnet::http::http_simple_url simple_url(queue, url);

  auto handler = [](const winnet::http::simple_request &request,
                    winnet::http::simple_response &response) {
    // handler for testing
    PHTTP_REQUEST req = request.get_request();
    BOOST_LOG_TRIVIAL(debug)
        << L"Got a request for url: " << req->CookedUrl.pFullUrl << L" VerbId: "
        << static_cast<int>(req->Verb);
    BOOST_LOG_TRIVIAL(debug) << request;
    response.set_status_code(200);
    response.set_reason("OK");
    response.set_content_type("text/html");
    response.set_body("Hey! You hit the server \r\n");
    response.add_unknown_header("myRespHeader", "myRespHeaderVal");
    response.add_trailer("myTrailer", "myTrailerVal");
  };

  std::make_shared<http_connection<net::io_context::executor_type>>(queue,
                                                                    handler)
      ->start();

  int server_count = 1;
  std::vector<std::thread> server_threads;

  for (int n = 0; n < server_count; ++n) {
    server_threads.emplace_back([&] { io_context.run(); });
  }

  // uncomment this for manual testing
  // io_context.run();

  // let server warm up
  std::this_thread::sleep_for(std::chrono::seconds(1));

  test_request req;
  test_response resp;
  req.add_header("myheader", "myval");
  // make client call
  ec = make_test_request(req, resp);
  ASSERT_FALSE(ec.failed());
  ASSERT_EQ(resp.status, http::status::ok);

  // let server listen again
  std::this_thread::sleep_for(std::chrono::seconds(1));

  io_context.stop();
  for (auto &thread : server_threads) {
    if (thread.joinable()) {
      thread.join();
    }
  }
}