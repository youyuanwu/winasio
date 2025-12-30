//
// Copyright (c) 2022 Youyuan Wu
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/ut.hpp>

#include "boost/asio.hpp"
#include "boost/winasio/winhttp/winhttp.hpp"

#include "boost/winasio/winhttp/client.hpp"
#include "boost/winasio/winhttp/winhttp_stream.hpp"

// use beast server for testing
#include "beast_test_server.hpp"

#include <latch>

namespace net = boost::asio; // from <boost/asio.hpp>
namespace winnet = boost::winasio;

std::string wstring_convert(const std::wstring &wstr) {
  // TODO: use fmt.
  if (wstr.empty())
    return std::string();
  int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(),
                                        (int)wstr.size(), NULL, 0, NULL, NULL);
  std::string result(size_needed, 0);
  WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &result[0],
                      size_needed, NULL, NULL);
  return result;
}

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
    spdlog::info("Beast server started");
  }

  ~beast_server() { 
    ioc_.stop();
    spdlog::info("Beast server stopped");
    th_.join();
  }

private:
  std::latch lch_;
  net::io_context ioc_;
  std::jthread th_;
};

boost::ut::suite errors = [] {
  using namespace boost::ut;
  spdlog::set_level(spdlog::level::debug);
  "Basic"_test = [] {
    spdlog::info("Starting Basic test");
    beast_server bs;

    boost::system::error_code ec;
    net::io_context io_context;
    winnet::winhttp::basic_winhttp_session_handle<
        net::io_context::executor_type>
        h_session(io_context);
    h_session.open(ec); // by default async session is created
    expect(!ec.failed() >> fatal);

    winnet::winhttp::url_component url;
    url.crack(L"http://localhost:12345/count", ec);
    expect(!ec.failed() >> fatal);
    winnet::winhttp::basic_winhttp_connect_handle<
        net::io_context::executor_type>
        h_connect(io_context);
    h_connect.connect(h_session.native_handle(), url.get_hostname().c_str(),
                      url.get_port(), ec);
    expect(!ec.failed() >> fatal);

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
          spdlog::debug("async_exec handler: status: {}", ec.message());
          expect(!ec.failed() >> fatal) << "async_exec failed: " << ec.message();

          // print result
          std::wstring headers;
          winnet::winhttp::header::get_all_raw_crlf(h_request, ec, headers);
          expect(!ec.failed() >> fatal);
          spdlog::debug(L"Headers: {}", headers);
          spdlog::debug("Body: {}", winnet::winhttp::buff_to_string(buff));

          // more tests
          // check status
          DWORD dwStatusCode;
          winnet::winhttp::header::get_status_code(h_request, ec, dwStatusCode);
          expect(!ec.failed() >> fatal);

          // sometimes the api is 403, but this does not impact this test.
          expect(200 == dwStatusCode >> fatal);

          std::wstring version;
          winnet::winhttp::header::get_version(h_request, ec, version);
          expect(!ec.failed() >> fatal);
          expect(wstring_convert(version) == std::string("HTTP/1.1") >> fatal);
          std::wstring content_type;
          winnet::winhttp::header::get_content_type(h_request, ec,
                                                    content_type);
          expect(!ec.failed() >> fatal);
          expect(wstring_convert(content_type) ==
                 std::string("text/html") >> fatal);
        });

    io_context.run();
  };

  "ObjectHandleAlreadySet"_test = [] {
    // Tests when event is already set, executor runs the task immediately.
    net::io_context io_context;
    net::windows::object_handle oh(io_context);

    HANDLE ev = CreateEvent(NULL,  // default security attributes
                            TRUE,  // manual-reset event
                            FALSE, // initial state is nonsignaled
                            NULL   // object name
    );
    expect(ev != nullptr >> fatal);
    oh.assign(ev);

    bool ok = SetEvent(ev);
    expect(ok >> fatal);
    bool flag = false;
    oh.async_wait([&flag](boost::system::error_code ec) {
      expect(!ec.failed() >> fatal);
      flag = true;
    });

    io_context.run();
    expect(flag >> fatal);

    io_context.restart();
    oh.async_wait([&flag](boost::system::error_code ec) {
      expect(!ec.failed() >> fatal);
      flag = false;
    });
    io_context.run();
    expect(!flag >> fatal);
  };

  // "Coroutine"_test = [] {
  //   spdlog::info("Starting Coroutine test");
  //   beast_server bs;

  //   boost::system::error_code ec;
  //   net::io_context io_context;
  //   winnet::winhttp::basic_winhttp_session_handle<
  //       net::io_context::executor_type>
  //       h_session(io_context);
  //   h_session.open(ec); // by default async session is created
  //   expect(!ec.failed() >> fatal);

  //   winnet::winhttp::url_component url;
  //   url.crack(L"http://localhost:12345/count", ec);
  //   expect(!ec.failed() >> fatal);
  //   winnet::winhttp::basic_winhttp_connect_handle<
  //       net::io_context::executor_type>
  //       h_connect(io_context);
  //   h_connect.connect(h_session.native_handle(), url.get_hostname().c_str(),
  //                     url.get_port(), ec);
  //   expect(!ec.failed() >> fatal);

  //   // test GET request
  //   winnet::winhttp::basic_winhttp_request_asio_handle<
  //       net::io_context::executor_type>
  //       h_request1(io_context.get_executor());
  //   {
  //     std::wstring path = url.get_path();
  //     h_request1.managed_open(h_connect.native_handle(), L"GET", path.c_str(),
  //                             NULL, // http 1.1
  //                             WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
  //                             NULL, // no ssl WINHTTP_FLAG_SECURE,
  //                             ec);
  //     expect(!ec.failed() >> fatal);

  //     auto f = [&h_request = h_request1, &url]() -> net::awaitable<void> {
  //       auto executor = co_await net::this_coro::executor;
  //       boost::system::error_code ec;

  //       co_await h_request.async_send(
  //           NULL,                    // headers
  //           0,                       // header len
  //           WINHTTP_NO_REQUEST_DATA, // optional
  //           0,                       // optional len
  //           0,                       // total len
  //           net::redirect_error(net::use_awaitable, ec));
  //       expect(!ec.failed() >> fatal);
  //       std::vector<BYTE> body_buff;
  //       auto dybuff = net::dynamic_buffer(body_buff);

  //       co_await h_request.async_recieve_response(
  //           net::redirect_error(net::use_awaitable, ec));
  //       expect(!ec.failed() >> fatal);
  //       std::size_t len = 0;
  //       do {
  //         len = co_await h_request.async_query_data_available(
  //             net::redirect_error(net::use_awaitable, ec));
  //         expect(!ec.failed() >> fatal);
  //         auto buff = dybuff.prepare(len + 1);
  //         std::size_t read_len = co_await h_request.async_read_data(
  //             (LPVOID)buff.data(), static_cast<DWORD>(len),
  //             net::redirect_error(net::use_awaitable, ec));
  //         expect(!ec.failed() >> fatal);
  //         expect(read_len == len >> fatal);
  //         dybuff.commit(len);
  //       } while (len > 0);
  //       spdlog::debug("request body:");
  //       spdlog::debug(winnet::winhttp::buff_to_string(dybuff));
  //     };
  //     net::co_spawn(io_context, f, net::detached);
  //   }

  //   // test POST request
  //   winnet::winhttp::basic_winhttp_request_asio_handle<
  //       net::io_context::executor_type>
  //       h_request2(io_context.get_executor());
  //   {
  //     std::wstring path = url.get_path();
  //     h_request2.managed_open(h_connect.native_handle(), L"POST", path.c_str(),
  //                             NULL, // http 1.1
  //                             WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
  //                             NULL, // no ssl WINHTTP_FLAG_SECURE,
  //                             ec);
  //     expect(!ec.failed() >> fatal);

  //     auto f = [&h_request2, &url]() -> net::awaitable<void> {
  //       auto executor = co_await net::this_coro::executor;
  //       boost::system::error_code ec;

  //       std::string req_body = "set_count=100";
  //       const DWORD req_len = static_cast<DWORD>(req_body.size());
  //       co_await h_request2.async_send(
  //           NULL,    // headers
  //           0,       // header len
  //           NULL,    // optional
  //           0,       // optional len
  //           req_len, // total len
  //           net::redirect_error(net::use_awaitable, ec));
  //       expect(!ec.failed() >> fatal);
  //       std::size_t total_write_len = {};
  //       std::size_t index = {};
  //       std::size_t remain_len = req_len;
  //       while (total_write_len < req_len) {
  //         char *c = req_body.data() + index;
  //         std::size_t write_len = co_await h_request2.async_write_data(
  //             (LPCVOID)c, static_cast<DWORD>(remain_len),
  //             net::redirect_error(net::use_awaitable, ec));
  //         expect(!ec.failed() >> fatal);
  //         total_write_len += write_len;
  //         index += write_len;
  //         remain_len -= write_len;
  //         // This is not necessary the case since winhttp might not gurantee
  //         // that write len equals provided buffer len. If test fail here, we
  //         // can remove this check.
  //         expect(req_len == write_len >> fatal);
  //       }
  //       expect(req_len == total_write_len >> fatal);

  //       std::vector<BYTE> body_buff;
  //       auto dybuff = net::dynamic_buffer(body_buff);

  //       co_await h_request2.async_recieve_response(
  //           net::redirect_error(net::use_awaitable, ec));
  //       expect(!ec.failed() >> fatal);
  //       std::size_t len = 0;
  //       do {
  //         len = co_await h_request2.async_query_data_available(
  //             net::redirect_error(net::use_awaitable, ec));
  //         expect(!ec.failed() >> fatal);
  //         auto buff = dybuff.prepare(len + 1);
  //         std::size_t read_len = co_await h_request2.async_read_data(
  //             (LPVOID)buff.data(), static_cast<DWORD>(len),
  //             net::redirect_error(net::use_awaitable, ec));
  //         expect(!ec.failed() >> fatal);
  //         expect(read_len == len >> fatal);
  //         dybuff.commit(len);
  //       } while (len > 0);
  //       spdlog::debug("request body:");
  //       spdlog::debug(winnet::winhttp::buff_to_string(dybuff));
  //     };
  //     net::co_spawn(io_context, f, net::detached);
  //   }

  //   // stress test async by read and write one byte at a time.
  //   winnet::winhttp::basic_winhttp_request_asio_handle<
  //       net::io_context::executor_type>
  //       h_request3(io_context.get_executor());
  //   {
  //     std::wstring path = url.get_path();
  //     h_request3.managed_open(h_connect.native_handle(), L"POST", path.c_str(),
  //                             NULL, // http 1.1
  //                             WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
  //                             NULL, // no ssl WINHTTP_FLAG_SECURE,
  //                             ec);
  //     expect(!ec.failed() >> fatal);

  //     auto f = [&h_request = h_request3, &url]() -> net::awaitable<void> {
  //       auto executor = co_await net::this_coro::executor;
  //       boost::system::error_code ec;
  //       std::string req_body = "set_count=111";
  //       const DWORD req_len = static_cast<DWORD>(req_body.size());
  //       co_await h_request.async_send(
  //           NULL,    // headers
  //           0,       // header len
  //           NULL,    // optional
  //           0,       // optional len
  //           req_len, // total len
  //           net::redirect_error(net::use_awaitable, ec));
  //       expect(!ec.failed() >> fatal);

  //       // write data byte by byte to stress test
  //       std::size_t total_write_len = {};
  //       std::size_t index = {};
  //       const DWORD step_size = 1;
  //       while (total_write_len < req_len) {
  //         char *c = req_body.data() + index;
  //         std::size_t write_len = co_await h_request.async_write_data(
  //             (LPCVOID)c, step_size,
  //             net::redirect_error(net::use_awaitable, ec));
  //         expect(!ec.failed() >> fatal);
  //         total_write_len += write_len;
  //         index += write_len;
  //       }
  //       expect(req_len == total_write_len >> fatal);
  //       co_await h_request.async_recieve_response(
  //           net::redirect_error(net::use_awaitable, ec));
  //       expect(!ec.failed() >> fatal);

  //       DWORD dwStatusCode = {};
  //       winnet::winhttp::header::get_status_code(h_request, ec, dwStatusCode);
  //       expect(!ec.failed() >> fatal);
  //       expect(static_cast<DWORD>(201) == dwStatusCode >> fatal);

  //       // read bytes one by one
  //       std::size_t total_read = {};
  //       std::string body = {};
  //       while (true) {
  //         std::size_t len = co_await h_request.async_query_data_available(
  //             net::redirect_error(net::use_awaitable, ec));
  //         expect(!ec.failed() >> fatal);
  //         if (len == 0) {
  //           break;
  //         }
  //         char c = {};
  //         std::size_t read_len = co_await h_request.async_read_data(
  //             (LPVOID)&c, 1, net::redirect_error(net::use_awaitable, ec));
  //         expect(!ec.failed() >> fatal);
  //         total_read += read_len;
  //         body += c;
  //       }
  //       expect(total_read == body.size() >> fatal);
  //       spdlog::debug("request body:");
  //       spdlog::debug(body);
  //     };
  //     net::co_spawn(io_context, f, net::detached);
  //   }

  //   // test using stream
  //   winnet::winhttp::basic_winhttp_request_stream_handle<
  //       net::io_context::executor_type>
  //       h_stream(io_context.get_executor());
  //   {
  //     std::wstring path = url.get_path();
  //     h_stream.managed_open(h_connect.native_handle(), L"POST", path.c_str(),
  //                           NULL, // http 1.1
  //                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
  //                           NULL, // no ssl WINHTTP_FLAG_SECURE,
  //                           ec);
  //     expect(ec == boost::system::errc::success >> fatal);

  //     auto f = [&h_request = h_stream, &url]() -> net::awaitable<void> {
  //       auto executor = co_await net::this_coro::executor;
  //       boost::system::error_code ec;
  //       std::string req_body = "set_count=200";
  //       DWORD req_len = static_cast<DWORD>(req_body.size());
  //       co_await h_request.async_send(
  //           NULL,                                // headers
  //           0,                                   // header len
  //           NULL,                                // optional
  //           0,                                   // optional len
  //           static_cast<DWORD>(req_body.size()), // total len
  //           net::redirect_error(net::use_awaitable, ec));
  //       expect(!ec.failed() >> fatal);

  //       std::size_t write_len = co_await net::async_write(
  //           h_request, net::buffer(req_body),
  //           net::redirect_error(net::use_awaitable, ec));
  //       expect(!ec.failed() >> fatal);

  //       expect(req_len == write_len >> fatal);

  //       co_await h_request.async_recieve_response(
  //           net::redirect_error(net::use_awaitable, ec));
  //       expect(!ec.failed() >> fatal);
  //       std::vector<BYTE> body_buff;
  //       co_await net::async_read(h_request, net::dynamic_buffer(body_buff),
  //                                net::redirect_error(net::use_awaitable, ec));
  //       expect(!ec.failed() >> fatal);
  //       spdlog::debug("request body:");
  //       spdlog::debug(std::string(body_buff.begin(), body_buff.end()));
  //     };
  //     net::co_spawn(io_context, f, net::detached);
  //   }

  //   io_context.run();
  // };
};

int main() {}