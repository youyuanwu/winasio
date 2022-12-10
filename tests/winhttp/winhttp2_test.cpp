#include "boost/asio.hpp"
#include "boost/winasio/winhttp/winhttp.hpp"
#include "gtest/gtest.h"

#include "boost/winasio/winhttp/client.hpp"

#include <boost/log/trivial.hpp>

namespace net = boost::asio; // from <boost/asio.hpp>
namespace winnet = boost::winasio;

TEST(HTTPClient, Basic) {
  boost::system::error_code ec;
  net::io_context io_context;
  winnet::winhttp::basic_winhttp_session_handle<net::io_context::executor_type>
      h_session(io_context);
  h_session.open(ec); // by default async session is created
  ASSERT_FALSE(ec.failed());

  winnet::winhttp::url_component url;
  url.crack(L"https://api.github.com:443", ec);
  ASSERT_FALSE(ec.failed());

  winnet::winhttp::basic_winhttp_connect_handle<net::io_context::executor_type>
      h_connect(io_context);
  h_connect.connect(h_session.native_handle(), url.get_hostname().c_str(),
                    url.get_port(), ec);
  ASSERT_FALSE(ec.failed());

  winnet::winhttp::payload pl;
  pl.method = L"GET";
  pl.path = std::nullopt;
  pl.header = std::nullopt;
  pl.accept = std::nullopt;
  pl.body = std::nullopt;
  pl.secure = true;

  winnet::winhttp::basic_winhttp_request_asio_handle<
      net::io_context::executor_type>
      h_request(io_context.get_executor());

  std::vector<BYTE> body_buff;
  auto buff = net::dynamic_buffer(body_buff);
  winnet::winhttp::async_exec(
      pl, h_connect, h_request, buff,
      [&h_request, &buff](boost::system::error_code ec, std::size_t) {
        BOOST_LOG_TRIVIAL(debug) << "async_exec handler";
        ASSERT_FALSE(ec.failed());

        // print result
        std::wstring headers;
        winnet::winhttp::header::get_all_raw_crlf(h_request, ec, headers);
        ASSERT_FALSE(ec.failed());
        BOOST_LOG_TRIVIAL(debug) << headers;
        BOOST_LOG_TRIVIAL(debug) << winnet::winhttp::buff_to_string(buff);

        // more tests
        // check status
        DWORD dwStatusCode;
        winnet::winhttp::header::get_status_code(h_request, ec, dwStatusCode);
        ASSERT_EQ(boost::system::errc::success, ec);

        // sometimes the api is 403, but this does not impact this test.
        ASSERT_TRUE(200 == dwStatusCode || 403 == dwStatusCode);

        std::wstring version;
        winnet::winhttp::header::get_version(h_request, ec, version);
        ASSERT_EQ(boost::system::errc::success, ec);
        ASSERT_EQ(L"HTTP/1.1", version);
        std::wstring content_type;
        winnet::winhttp::header::get_content_type(h_request, ec, content_type);
        ASSERT_EQ(boost::system::errc::success, ec);
        ASSERT_EQ(L"application/json; charset=utf-8", content_type);
      });

  io_context.run();
}
