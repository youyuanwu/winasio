//
// Copyright (c) 2022 Youyuan Wu
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/ut.hpp>
#include <spdlog/spdlog.h>

#include <boost/winasio/http/http.hpp>
#include <boost/winasio/http/temp.hpp>

#include "beast_client.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#pragma warning(push)
#pragma warning(disable : 4702)
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#pragma warning(pop)

#include <chrono>
#include <cstdio>
#include <utility>

using namespace std::chrono_literals;

namespace beast = boost::beast; // from <boost/beast.hpp>
namespace http = beast::http;   // from <boost/beast/http.hpp>
namespace net = boost::asio;    // from <boost/asio.hpp>
using tcp = net::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

namespace winnet = boost::winasio;

template <winnet::http::HTTP_MAJOR_VERSION http_version>
void http_server_test_helper() {
  // init http module
  winnet::http::http_initializer<http_version> init;
  // add https then this becomes https server
  std::wstring url = L"http://localhost:12356/winhttpapitest/";

  boost::system::error_code ec;
  net::io_context io_context;

  // open queue handle
  winnet::http::basic_http_queue_handle<net::io_context::executor_type> queue(
      io_context);
  queue.assign(init.create_http_queue(ec));
  BOOST_ASSERT(!ec.failed());
  winnet::http::basic_http_url<net::io_context::executor_type, http_version>
      simple_url(queue, url);

  auto handler = [](const winnet::http::simple_request &request,
                    winnet::http::simple_response &response) {
    // handler for testing
    PHTTP_REQUEST req = request.get_request();
    spdlog::debug(L"Got a request for url: {} VerbId: {}",
                  req->CookedUrl.pFullUrl, static_cast<int>(req->Verb));
    spdlog::debug("{}", request.get_body_string());
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
  req.verb_ = http::verb::post;
  req.set_body("myRequestBody");
  req.add_header("myheader", "myval");
  // make client call
  ec = make_test_request(req, resp);
  BOOST_ASSERT(!ec.failed());
  boost::ut::expect(resp.status == http::status::ok);

  // let server listen again
  std::this_thread::sleep_for(std::chrono::seconds(1));

  io_context.stop();
  for (auto &thread : server_threads) {
    if (thread.joinable()) {
      thread.join();
    }
  }
}

template <winnet::http::HTTP_MAJOR_VERSION http_version>
void server_gracefully_shutdown_helper() {
  // init http module
  winnet::http::http_initializer<http_version> init;
  // add https then this becomes https server
  std::wstring url = L"http://localhost:12356/winhttpapitest/";

  boost::system::error_code ec;
  net::io_context io_context;

  // open queue handle
  winnet::http::basic_http_queue_handle<net::io_context::executor_type> queue(
      io_context);
  queue.assign(init.create_http_queue(ec));
  BOOST_ASSERT(!ec.failed());
  winnet::http::basic_http_url<net::io_context::executor_type, http_version>
      simple_url(queue, url);

  winnet::http::simple_request rq;
  boost::system::error_code cncl_ec;
  winnet::http::async_receive(queue, rq.get_request_dynamic_buffer(),
                              [&cncl_ec](const boost::system::error_code &ec,
                                         size_t) { cncl_ec = ec; });

  std::thread t([&]() {
    std::this_thread::sleep_for(300ms);
    boost::system::error_code ec;
    queue.shutdown(ec);
  });

  io_context.run();

  t.join();

  boost::ut::expect(cncl_ec.value() == 995); // cancelled.
}

boost::ut::suite errors = [] {
  using namespace boost::ut;

  spdlog::set_level(spdlog::level::debug);

  "server_http_ver_1"_test = [] {
    spdlog::info("Starting server_http_ver_1 test");
    http_server_test_helper<winnet::http::HTTP_MAJOR_VERSION::http_ver_1>();
  };

  "server_http_ver_2"_test = [] {
    spdlog::info("Starting server_http_ver_2 test");
    http_server_test_helper<winnet::http::HTTP_MAJOR_VERSION::http_ver_2>();
  };

  "gracefully_shutdown_http_ver_1"_test = [] {
    spdlog::info("Starting gracefully_shutdown_http_ver_1 test");
    server_gracefully_shutdown_helper<
        winnet::http::HTTP_MAJOR_VERSION::http_ver_1>();
  };

  "gracefully_shutdown_http_ver_2"_test = [] {
    spdlog::info("Starting gracefully_shutdown_http_ver_2 test");
    server_gracefully_shutdown_helper<
        winnet::http::HTTP_MAJOR_VERSION::http_ver_2>();
  };

  "server_url_register_api"_test = [] {
    spdlog::info("Starting server_url_register_api test");
    // init http module
    winnet::http::http_initializer<winnet::http::HTTP_MAJOR_VERSION::http_ver_1>
        init;

    boost::system::error_code ec;
    net::io_context ctx;

    // open queue handle
    winnet::http::queue queue(ctx, winnet::http::open_raw_http_queue());

    winnet::http::controller controller(queue, L"http://localhost:1337/");

    controller.get(L"/url-123",
                   [](winnet::http::controller::request_context &ctx) {
                     ctx.response.set_body("Hello world");
                     ctx.response.set_status_code(200);
                   });

    controller.post(L"/url-123",
                    [](winnet::http::controller::request_context &ctx) {
                      ctx.response.set_body("Hello world");
                      ctx.response.set_status_code(201);
                    });

    controller.put(L"/url-123",
                   [](winnet::http::controller::request_context &ctx) {
                     ctx.response.set_body("Hello world");
                     ctx.response.set_status_code(204);
                   });

    controller.del(L"/url-123",
                   [](winnet::http::controller::request_context &ctx) {
                     ctx.response.set_body("Hello world");
                     ctx.response.set_status_code(200);
                   });

    controller.start();
    // ctx.run();
  };
};

int main() {}