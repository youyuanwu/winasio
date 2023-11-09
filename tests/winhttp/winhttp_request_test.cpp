//
// Copyright (c) 2022 Youyuan Wu
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// #define BOOST_TEST_MODULE http_client2
#include <boost/test/unit_test.hpp>

#include "boost/asio.hpp"
#include "boost/winasio/winhttp/winhttp.hpp"

#include "boost/winasio/winhttp/client.hpp"
#include "boost/winasio/winhttp/winhttp_stream.hpp"

// use beast server for testing
#include "beast_test_server.hpp"

#include <boost/log/trivial.hpp>
#include <latch>

namespace net = boost::asio; // from <boost/asio.hpp>
namespace winnet = boost::winasio;

class beast_server {
public:
  beast_server() : lch_(1), ioc_(1), th_() {
    my_program_state::set_request_count(0);
    th_ = std::jthread([this]() {
      // start beast server
      unsigned short port = static_cast<unsigned short>(12345);
      myacceptor acceptor{ioc_, {tcp::v4(), port}};
      mysocket socket{ioc_};
      http_server(acceptor, socket);
      lch_.count_down();
      ioc_.run();
    });
    // wait for server to run;
    lch_.wait();
  }

  ~beast_server() { ioc_.stop(); }

private:
  std::latch lch_;
  net::io_context ioc_;
  std::jthread th_;
};

BOOST_AUTO_TEST_SUITE(test_winhttp_request)

BOOST_AUTO_TEST_CASE(Basic) {
  beast_server bs;

  boost::system::error_code ec;
  net::io_context io_context;
  winnet::winhttp::basic_winhttp_session_handle<net::io_context::executor_type>
      h_session(io_context);
  h_session.open(ec); // by default async session is created
  BOOST_REQUIRE(!ec.failed());

  winnet::winhttp::url_component url;
  url.crack(L"http://localhost:12345/count", ec);
  BOOST_REQUIRE(!ec.failed());

  winnet::winhttp::basic_winhttp_connect_handle<net::io_context::executor_type>
      h_connect(io_context);
  h_connect.connect(h_session.native_handle(), url.get_hostname().c_str(),
                    url.get_port(), ec);
  BOOST_REQUIRE(!ec.failed());

  winnet::winhttp::payload pl;
  pl.method = L"GET";
  pl.path = url.get_path();
  pl.header = std::nullopt;
  pl.accept = std::nullopt;
  pl.body = std::nullopt;
  pl.secure = false;

  // test callback style
  winnet::winhttp::basic_winhttp_request_asio_handle<
      net::io_context::executor_type>
      h_request(io_context.get_executor());

  std::vector<BYTE> body_buff;
  auto buff = net::dynamic_buffer(body_buff);
  winnet::winhttp::async_exec(
      pl, h_connect, h_request, buff,
      [&h_request, &buff](boost::system::error_code ec, std::size_t) {
        BOOST_LOG_TRIVIAL(debug) << "async_exec handler";
        BOOST_REQUIRE(!ec.failed());

        // print result
        std::wstring headers;
        winnet::winhttp::header::get_all_raw_crlf(h_request, ec, headers);
        BOOST_REQUIRE(!ec.failed());
        BOOST_LOG_TRIVIAL(debug) << headers;
        BOOST_LOG_TRIVIAL(debug) << winnet::winhttp::buff_to_string(buff);

        // more tests
        // check status
        DWORD dwStatusCode;
        winnet::winhttp::header::get_status_code(h_request, ec, dwStatusCode);
        BOOST_REQUIRE_EQUAL(boost::system::errc::success, ec);

        // sometimes the api is 403, but this does not impact this test.
        BOOST_REQUIRE(200 == dwStatusCode);

        std::wstring version;
        winnet::winhttp::header::get_version(h_request, ec, version);
        BOOST_CHECK_EQUAL(boost::system::errc::success, ec);
        BOOST_CHECK(L"HTTP/1.1" == version);
        std::wstring content_type;
        winnet::winhttp::header::get_content_type(h_request, ec, content_type);
        BOOST_REQUIRE_EQUAL(boost::system::errc::success, ec);
        BOOST_CHECK(L"text/html" == content_type);
      });

  io_context.run();
}

BOOST_AUTO_TEST_CASE(ObjectHandleAlreadySet) {
  // Tests when event is already set, executor runs the task immediately.
  net::io_context io_context;
  net::windows::object_handle oh(io_context);

  HANDLE ev = CreateEvent(NULL,  // default security attributes
                          TRUE,  // manual-reset event
                          FALSE, // initial state is nonsignaled
                          NULL   // object name
  );
  BOOST_REQUIRE(ev != nullptr);
  oh.assign(ev);

  bool ok = SetEvent(ev);
  BOOST_REQUIRE(ok);

  bool flag = false;
  oh.async_wait([&flag](boost::system::error_code ec) {
    BOOST_CHECK(!ec);
    flag = true;
  });

  io_context.run();
  BOOST_CHECK(flag);

  io_context.reset();
  oh.async_wait([&flag](boost::system::error_code ec) {
    BOOST_CHECK(!ec);
    flag = false;
  });
  io_context.run();
  BOOST_CHECK(!flag);
}

BOOST_AUTO_TEST_CASE(Coroutine) {
  beast_server bs;

  boost::system::error_code ec;
  net::io_context io_context;
  winnet::winhttp::basic_winhttp_session_handle<net::io_context::executor_type>
      h_session(io_context);
  h_session.open(ec); // by default async session is created
  BOOST_REQUIRE(!ec.failed());

  winnet::winhttp::url_component url;
  url.crack(L"http://localhost:12345/count", ec);
  BOOST_REQUIRE(!ec.failed());

  winnet::winhttp::basic_winhttp_connect_handle<net::io_context::executor_type>
      h_connect(io_context);
  h_connect.connect(h_session.native_handle(), url.get_hostname().c_str(),
                    url.get_port(), ec);
  BOOST_REQUIRE(!ec.failed());

  // test GET request
  winnet::winhttp::basic_winhttp_request_asio_handle<
      net::io_context::executor_type>
      h_request1(io_context.get_executor());
  {
    std::wstring path = url.get_path();
    h_request1.managed_open(h_connect.native_handle(), L"GET", path.c_str(),
                            NULL, // http 1.1
                            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                            NULL, // no ssl WINHTTP_FLAG_SECURE,
                            ec);
    BOOST_REQUIRE(!ec);

    auto f = [&h_request = h_request1, &url]() -> net::awaitable<void> {
      auto executor = co_await net::this_coro::executor;
      boost::system::error_code ec;

      co_await h_request.async_send(
          NULL,                    // headers
          0,                       // header len
          WINHTTP_NO_REQUEST_DATA, // optional
          0,                       // optional len
          0,                       // total len
          net::redirect_error(net::use_awaitable, ec));
      BOOST_REQUIRE_EQUAL(ec, boost::system::errc::success);
      std::vector<BYTE> body_buff;
      auto dybuff = net::dynamic_buffer(body_buff);

      co_await h_request.async_recieve_response(
          net::redirect_error(net::use_awaitable, ec));
      BOOST_REQUIRE_EQUAL(ec, boost::system::errc::success);
      std::size_t len = 0;
      do {
        len = co_await h_request.async_query_data_available(
            net::redirect_error(net::use_awaitable, ec));
        BOOST_REQUIRE_EQUAL(ec, boost::system::errc::success);
        auto buff = dybuff.prepare(len + 1);
        std::size_t read_len = co_await h_request.async_read_data(
            (LPVOID)buff.data(), static_cast<DWORD>(len),
            net::redirect_error(net::use_awaitable, ec));
        BOOST_REQUIRE_EQUAL(ec, boost::system::errc::success);
        BOOST_REQUIRE_EQUAL(read_len, len);
        dybuff.commit(len);
      } while (len > 0);
      BOOST_TEST_MESSAGE("request body:");
      BOOST_TEST_MESSAGE(winnet::winhttp::buff_to_string(dybuff));
    };
    net::co_spawn(io_context, f, net::detached);
  }

  // test POST request
  winnet::winhttp::basic_winhttp_request_asio_handle<
      net::io_context::executor_type>
      h_request2(io_context.get_executor());
  {
    std::wstring path = url.get_path();
    h_request2.managed_open(h_connect.native_handle(), L"POST", path.c_str(),
                            NULL, // http 1.1
                            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                            NULL, // no ssl WINHTTP_FLAG_SECURE,
                            ec);
    BOOST_REQUIRE_EQUAL(ec, boost::system::errc::success);

    auto f = [&h_request2, &url]() -> net::awaitable<void> {
      auto executor = co_await net::this_coro::executor;
      boost::system::error_code ec;

      std::string req_body = "set_count=100";
      const DWORD req_len = static_cast<DWORD>(req_body.size());
      co_await h_request2.async_send(
          NULL,    // headers
          0,       // header len
          NULL,    // optional
          0,       // optional len
          req_len, // total len
          net::redirect_error(net::use_awaitable, ec));
      BOOST_REQUIRE_EQUAL(ec, boost::system::errc::success);
      std::size_t total_write_len = {};
      std::size_t index = {};
      std::size_t remain_len = req_len;
      while (total_write_len < req_len) {
        char *c = req_body.data() + index;
        std::size_t write_len = co_await h_request2.async_write_data(
            (LPCVOID)c, static_cast<DWORD>(remain_len),
            net::redirect_error(net::use_awaitable, ec));
        BOOST_REQUIRE_EQUAL(ec, boost::system::errc::success);
        total_write_len += write_len;
        index += write_len;
        remain_len -= write_len;
        // This is not necessary the case since winhttp might not gurantee
        // that write len equals provided buffer len. If test fail here, we
        // can remove this check.
        BOOST_REQUIRE_EQUAL(req_len, write_len);
      }
      BOOST_REQUIRE_EQUAL(req_len, total_write_len);

      std::vector<BYTE> body_buff;
      auto dybuff = net::dynamic_buffer(body_buff);

      co_await h_request2.async_recieve_response(
          net::redirect_error(net::use_awaitable, ec));
      BOOST_REQUIRE_EQUAL(ec, boost::system::errc::success);
      std::size_t len = 0;
      do {
        len = co_await h_request2.async_query_data_available(
            net::redirect_error(net::use_awaitable, ec));
        BOOST_REQUIRE_EQUAL(ec, boost::system::errc::success);
        auto buff = dybuff.prepare(len + 1);
        std::size_t read_len = co_await h_request2.async_read_data(
            (LPVOID)buff.data(), static_cast<DWORD>(len),
            net::redirect_error(net::use_awaitable, ec));
        BOOST_REQUIRE_EQUAL(ec, boost::system::errc::success);
        BOOST_REQUIRE_EQUAL(read_len, len);
        dybuff.commit(len);
      } while (len > 0);
      BOOST_TEST_MESSAGE("request body:");
      BOOST_TEST_MESSAGE(winnet::winhttp::buff_to_string(dybuff));
    };
    net::co_spawn(io_context, f, net::detached);
  }

  // stress test async by read and write one byte at a time.
  winnet::winhttp::basic_winhttp_request_asio_handle<
      net::io_context::executor_type>
      h_request3(io_context.get_executor());
  {
    std::wstring path = url.get_path();
    h_request3.managed_open(h_connect.native_handle(), L"POST", path.c_str(),
                            NULL, // http 1.1
                            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                            NULL, // no ssl WINHTTP_FLAG_SECURE,
                            ec);
    BOOST_REQUIRE_EQUAL(ec, boost::system::errc::success);

    auto f = [&h_request = h_request3, &url]() -> net::awaitable<void> {
      auto executor = co_await net::this_coro::executor;
      boost::system::error_code ec;
      std::string req_body = "set_count=111";
      const DWORD req_len = static_cast<DWORD>(req_body.size());
      co_await h_request.async_send(
          NULL,    // headers
          0,       // header len
          NULL,    // optional
          0,       // optional len
          req_len, // total len
          net::redirect_error(net::use_awaitable, ec));
      BOOST_REQUIRE_EQUAL(ec, boost::system::errc::success);

      // write data byte by byte to stress test
      std::size_t total_write_len = {};
      std::size_t index = {};
      const DWORD step_size = 1;
      while (total_write_len < req_len) {
        char *c = req_body.data() + index;
        std::size_t write_len = co_await h_request.async_write_data(
            (LPCVOID)c, step_size, net::redirect_error(net::use_awaitable, ec));
        BOOST_REQUIRE_EQUAL(ec, boost::system::errc::success);
        total_write_len += write_len;
        index += write_len;
      }
      BOOST_REQUIRE_EQUAL(req_len, total_write_len);

      co_await h_request.async_recieve_response(
          net::redirect_error(net::use_awaitable, ec));
      BOOST_REQUIRE_EQUAL(ec, boost::system::errc::success);

      DWORD dwStatusCode = {};
      winnet::winhttp::header::get_status_code(h_request, ec, dwStatusCode);
      BOOST_CHECK_EQUAL(boost::system::errc::success, ec);
      BOOST_CHECK_EQUAL(static_cast<DWORD>(201), dwStatusCode);

      // read bytes one by one
      std::size_t total_read = {};
      std::string body = {};
      while (true) {
        std::size_t len = co_await h_request.async_query_data_available(
            net::redirect_error(net::use_awaitable, ec));
        BOOST_REQUIRE_EQUAL(ec, boost::system::errc::success);
        if (len == 0) {
          break;
        }
        char c = {};
        std::size_t read_len = co_await h_request.async_read_data(
            (LPVOID)&c, 1, net::redirect_error(net::use_awaitable, ec));
        BOOST_REQUIRE_EQUAL(ec, boost::system::errc::success);
        total_read += read_len;
        body += c;
      }
      BOOST_CHECK_EQUAL(total_read, body.size());

      BOOST_TEST_MESSAGE("request body:");
      BOOST_TEST_MESSAGE(body);
    };
    net::co_spawn(io_context, f, net::detached);
  }

  // test using stream
  winnet::winhttp::basic_winhttp_request_stream_handle<
      net::io_context::executor_type>
      h_stream(io_context.get_executor());
  {
    std::wstring path = url.get_path();
    h_stream.managed_open(h_connect.native_handle(), L"POST", path.c_str(),
                          NULL, // http 1.1
                          WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                          NULL, // no ssl WINHTTP_FLAG_SECURE,
                          ec);
    BOOST_REQUIRE_EQUAL(ec, boost::system::errc::success);

    auto f = [&h_request = h_stream, &url]() -> net::awaitable<void> {
      auto executor = co_await net::this_coro::executor;
      boost::system::error_code ec;
      std::string req_body = "set_count=200";
      DWORD req_len = static_cast<DWORD>(req_body.size());
      co_await h_request.async_send(
          NULL,                                // headers
          0,                                   // header len
          NULL,                                // optional
          0,                                   // optional len
          static_cast<DWORD>(req_body.size()), // total len
          net::redirect_error(net::use_awaitable, ec));
      BOOST_REQUIRE_EQUAL(ec, boost::system::errc::success);

      std::size_t write_len = co_await net::async_write(
          h_request, net::buffer(req_body),
          net::redirect_error(net::use_awaitable, ec));
      BOOST_REQUIRE_EQUAL(ec, boost::system::errc::success);

      BOOST_REQUIRE_EQUAL(req_len, write_len);

      co_await h_request.async_recieve_response(
          net::redirect_error(net::use_awaitable, ec));
      BOOST_REQUIRE_EQUAL(ec, boost::system::errc::success);

      std::vector<BYTE> body_buff;
      co_await net::async_read(h_request, net::dynamic_buffer(body_buff),
                               net::redirect_error(net::use_awaitable, ec));
      BOOST_REQUIRE_EQUAL(ec, boost::system::errc::success);
      BOOST_TEST_MESSAGE("request body:");
      BOOST_TEST_MESSAGE(std::string(body_buff.begin(), body_buff.end()));
    };
    net::co_spawn(io_context, f, net::detached);
  }

  io_context.run();
}

BOOST_AUTO_TEST_SUITE_END()