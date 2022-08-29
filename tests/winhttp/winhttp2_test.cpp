#include "boost/asio.hpp"
#include "boost/winasio/winhttp/winhttp.hpp"
#include "gtest/gtest.h"

#include "boost/winasio/winhttp/client.hpp"

namespace net = boost::asio; // from <boost/asio.hpp>
namespace winnet = boost::winasio;

TEST(HTTPClient, Basic) {
  boost::system::error_code ec;
  net::io_context io_context;
  winnet::http::basic_winhttp_session_handle<net::io_context::executor_type>
      h_session(io_context);
  h_session.open(ec); // by default async session is created
  ASSERT_FALSE(ec.failed());

  winnet::http::url_component url;
  url.crack(L"https://api.github.com:443", ec);
  ASSERT_FALSE(ec.failed());

  winnet::http::basic_winhttp_connect_handle<net::io_context::executor_type>
      h_connect(io_context);
  h_connect.connect(h_session.native_handle(), url.get_hostname().c_str(),
                    url.get_port(), ec);
  ASSERT_FALSE(ec.failed());

  const std::wstring method = L"GET";
  const std::optional<std::wstring> path = std::nullopt;
  const winnet::http::header::headers hd;         // empty
  winnet::http::header::accept_types accept = {}; // empty
  const std::optional<std::string> body = std::nullopt;

  auto request =
      std::make_shared<winnet::http::request<net::io_context::executor_type>>(
          io_context.get_executor());

  request->open(h_connect, method, path, accept, ec);
  ASSERT_FALSE(ec.failed());

  request->async_exec(
      hd, body,
      [](boost::system::error_code ec,
         winnet::http::response<net::io_context::executor_type> &resp) {
        BOOST_LOG_TRIVIAL(debug) << "async_exec handler";
        ASSERT_FALSE(ec.failed());

        auto &h_request = resp.get_handle();
        // print result
        std::wstring headers;
        winnet::http::header::get_all_raw_crlf(h_request, ec, headers);
        ASSERT_FALSE(ec.failed());
        BOOST_LOG_TRIVIAL(debug) << headers;
        BOOST_LOG_TRIVIAL(debug) << resp.get_body_str();

        // more tests
        // check status
        DWORD dwStatusCode;
        winnet::http::header::get_status_code(h_request, ec, dwStatusCode);
        ASSERT_EQ(boost::system::errc::success, ec);
        ASSERT_EQ(200, dwStatusCode);
        std::wstring version;
        winnet::http::header::get_version(h_request, ec, version);
        ASSERT_EQ(boost::system::errc::success, ec);
        ASSERT_EQ(L"HTTP/1.1", version);
        std::wstring content_type;
        winnet::http::header::get_content_type(h_request, ec, content_type);
        ASSERT_EQ(boost::system::errc::success, ec);
        ASSERT_EQ(L"application/json; charset=utf-8", content_type);
      });

  // wait for event
  io_context.run();
}