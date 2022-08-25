#include "boost/asio.hpp"
#include "boost/winasio/winhttp/winhttp.hpp"
#include "gtest/gtest.h"

#include <iostream>

namespace net = boost::asio; // from <boost/asio.hpp>
namespace winnet = boost::winasio;

// TEST(HTTPClient, MyTest) {
TEST(HTTPClient, DISABLED_MyTest) {
  boost::system::error_code ec;
  net::io_context io_context;

  winnet::http::basic_winhttp_session_handle<net::io_context::executor_type>
      h_session(io_context);
  h_session.open(ec); // by default async session is created
  ASSERT_FALSE(ec.failed());

  std::vector<BYTE> data(0);
  auto buff = net::dynamic_buffer(data);

  winnet::http::url_component url;
  url.crack(L"https://api.github.com:443", ec);
  ASSERT_FALSE(ec.failed());

  winnet::http::REQUEST_CONTEXT rcContext(io_context.get_executor(), buff);

  winnet::http::basic_winhttp_connect_handle<net::io_context::executor_type>
      h_connect(io_context);
  h_connect.connect(h_session.native_handle(), url.get_hostname().c_str(),
                    url.get_port(), ec);
  ASSERT_FALSE(ec.failed());

  rcContext.h_request.open(h_connect.native_handle(), L"GET", NULL, ec);
  ASSERT_FALSE(ec.failed());

  rcContext.h_request.set_status_callback(
      (WINHTTP_STATUS_CALLBACK)winnet::http::AsyncCallback<decltype(buff)>, ec);
  ASSERT_FALSE(ec.failed());

  rcContext.h_request.send(WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                           WINHTTP_NO_REQUEST_DATA, 0, 0, (DWORD_PTR)&rcContext,
                           ec);
  ASSERT_FALSE(ec.failed());

  // wait for event
  io_context.run();

  ASSERT_EQ(boost::system::errc::success, rcContext.ec);

  // check status
  DWORD dwStatusCode;
  winnet::http::header::get_status_code(rcContext.h_request, ec, dwStatusCode);
  ASSERT_EQ(boost::system::errc::success, ec);
  ASSERT_EQ(200, dwStatusCode);
  std::wstring version;
  winnet::http::header::get_version(rcContext.h_request, ec, version);
  ASSERT_EQ(boost::system::errc::success, ec);
  ASSERT_EQ(L"HTTP/1.1", version);
  std::wstring content_type;
  winnet::http::header::get_content_type(rcContext.h_request, ec, content_type);
  ASSERT_EQ(boost::system::errc::success, ec);
  ASSERT_EQ(L"application/json; charset=utf-8", content_type);
  // print result;
  BOOST_LOG_TRIVIAL(debug) << std::string(data.begin(), data.end());
}

TEST(HTTPClient, CrackURL) {
  boost::system::error_code ec;
  winnet::http::url_component x;
  x.crack(L"https://www.google.com:12345/person?name=john", ec);
  ASSERT_FALSE(ec.failed());
  ASSERT_EQ(L"www.google.com", x.get_hostname());
  ASSERT_EQ(INTERNET_SCHEME_HTTPS, x.get_nscheme());
  ASSERT_EQ(12345, x.get_port());
  ASSERT_EQ(L"/person", x.get_path());
  ASSERT_EQ(L"?name=john", x.get_query());
}

// exercise for using buffer.
TEST(HTTPClient, ASIOBuff) {
  std::vector<BYTE> data(0);
  auto buff = net::dynamic_buffer(data);

  ASSERT_EQ(0, buff.size());
  auto part = buff.prepare(5);
  std::size_t n = net::buffer_copy(part, net::const_buffer("hello", 5));
  ASSERT_EQ(5, n);
  ASSERT_EQ('h', data[0]);
  ASSERT_EQ(5, data.size());

  ASSERT_EQ(0, buff.size()); // not commited.
  buff.commit(n);
  ASSERT_EQ(5, buff.size());
}

TEST(HTTPClient, AcceptType) {
  winnet::http::header::accept_types at = {L"application/json"};
  const LPCWSTR *data = at.get();
  ASSERT_EQ(*data, std::wstring(L"application/json"));
  ASSERT_EQ(*(data + 1), nullptr);
}

TEST(HTTPClient, AdditionalHeaders) {

  winnet::http::header::headers hs;
  hs.add(L"myheader1", L"myval1").add(L"myheader2", L"myval2");

  ASSERT_EQ(L"myheader1: myval1\r\nmyheader2: myval2", std::wstring(hs.get()));

  winnet::http::header::headers hs0; // empty
  ASSERT_EQ(nullptr, hs0.get());
  ASSERT_EQ(0, hs0.size());
}