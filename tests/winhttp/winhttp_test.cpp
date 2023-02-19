#define BOOST_TEST_MODULE http_client
#include <boost/test/unit_test.hpp>

#include "boost/asio.hpp"
#include "boost/winasio/winhttp/winhttp.hpp"
// include temp impl/application of winhttp
// #include "boost\winasio\winhttp\temp.hpp"

#include <iostream>

namespace net = boost::asio; // from <boost/asio.hpp>
namespace winnet = boost::winasio;

// TEST(HTTPClient, MyTest) {
// TEST(HTTPClient, DISABLED_MyTest) {
//   boost::system::error_code ec;
//   net::io_context io_context;

//   winnet::winhttp::basic_winhttp_session_handle<net::io_context::executor_type>
//       h_session(io_context);
//   h_session.open(ec); // by default async session is created
//   ASSERT_FALSE(ec.failed());

//   std::vector<BYTE> data(0);
//   auto buff = net::dynamic_buffer(data);

//   winnet::winhttp::url_component url;
//   url.crack(L"https://api.github.com:443", ec);
//   ASSERT_FALSE(ec.failed());

//   winnet::winhttp::REQUEST_CONTEXT rcContext(io_context.get_executor(),
//   buff);

//   winnet::winhttp::basic_winhttp_connect_handle<net::io_context::executor_type>
//       h_connect(io_context);
//   h_connect.connect(h_session.native_handle(), url.get_hostname().c_str(),
//                     url.get_port(), ec);
//   ASSERT_FALSE(ec.failed());

//   rcContext.h_request.open(h_connect.native_handle(), L"GET", NULL, ec);
//   ASSERT_FALSE(ec.failed());

//   rcContext.h_request.set_status_callback(
//       (WINHTTP_STATUS_CALLBACK)winnet::winhttp::AsyncCallback<decltype(buff)>,
//       ec);
//   ASSERT_FALSE(ec.failed());

//   rcContext.h_request.send(WINHTTP_NO_ADDITIONAL_HEADERS, 0,
//                            WINHTTP_NO_REQUEST_DATA, 0, 0,
//                            (DWORD_PTR)&rcContext, ec);
//   ASSERT_FALSE(ec.failed());

//   // wait for event
//   io_context.run();

//   ASSERT_EQ(boost::system::errc::success, rcContext.ec);

//   // check status
//   DWORD dwStatusCode;
//   winnet::winhttp::header::get_status_code(rcContext.h_request, ec,
//                                            dwStatusCode);
//   ASSERT_EQ(boost::system::errc::success, ec);
//   ASSERT_EQ(200, dwStatusCode);
//   std::wstring version;
//   winnet::winhttp::header::get_version(rcContext.h_request, ec, version);
//   ASSERT_EQ(boost::system::errc::success, ec);
//   ASSERT_EQ(L"HTTP/1.1", version);
//   std::wstring content_type;
//   winnet::winhttp::header::get_content_type(rcContext.h_request, ec,
//                                             content_type);
//   ASSERT_EQ(boost::system::errc::success, ec);
//   ASSERT_EQ(L"application/json; charset=utf-8", content_type);
//   // print result;
//   BOOST_LOG_TRIVIAL(debug) << std::string(data.begin(), data.end());
// }

BOOST_AUTO_TEST_SUITE(test_http_client)

BOOST_AUTO_TEST_CASE(CrackURL) {
  boost::system::error_code ec;
  winnet::winhttp::url_component x;
  x.crack(L"https://www.google.com:12345/person?name=john", ec);
  BOOST_REQUIRE(!ec.failed());
  BOOST_CHECK(std::wstring(L"www.google.com") == x.get_hostname());
  BOOST_CHECK_EQUAL(INTERNET_SCHEME_HTTPS, x.get_nscheme());
  BOOST_CHECK_EQUAL(12345, x.get_port());
  BOOST_CHECK(std::wstring(L"/person") == x.get_path());
  BOOST_CHECK(std::wstring(L"?name=john") == x.get_query());
}

// exercise for using buffer.
BOOST_AUTO_TEST_CASE(ASIOBuff) {
  std::vector<BYTE> data(0);
  auto buff = net::dynamic_buffer(data);

  BOOST_CHECK_EQUAL(0, buff.size());
  auto part = buff.prepare(5);
  std::size_t n = net::buffer_copy(part, net::const_buffer("hello", 5));
  BOOST_CHECK_EQUAL(5, n);
  BOOST_CHECK_EQUAL('h', data[0]);
  BOOST_CHECK_EQUAL(5, data.size());

  BOOST_CHECK_EQUAL(0, buff.size()); // not commited.
  buff.commit(n);
  BOOST_CHECK_EQUAL(5, buff.size());
}

BOOST_AUTO_TEST_CASE(AcceptType) {
  winnet::winhttp::header::accept_types at = {L"application/json"};
  const LPCWSTR *data = at.get();
  BOOST_CHECK(std::wstring(*data) == std::wstring(L"application/json"));
  BOOST_CHECK_EQUAL(*(data + 1), nullptr);
}

BOOST_AUTO_TEST_CASE(AdditionalHeaders) {

  winnet::winhttp::header::headers hs;
  hs.add(L"myheader1", L"myval1").add(L"myheader2", L"myval2");

  BOOST_CHECK(std::wstring(L"myheader1: myval1\r\nmyheader2: myval2") ==
              std::wstring(hs.get()));

  winnet::winhttp::header::headers hs0; // empty
  BOOST_CHECK_EQUAL(nullptr, hs0.get());
  BOOST_CHECK_EQUAL(0u, hs0.size());
}

BOOST_AUTO_TEST_SUITE_END()