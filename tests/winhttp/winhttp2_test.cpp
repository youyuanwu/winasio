#define BOOST_TEST_MODULE http_client2
#include <boost/test/unit_test.hpp>

#include "boost/asio.hpp"
#include "boost/winasio/winhttp/winhttp.hpp"

#include "boost/winasio/winhttp/client.hpp"

// use beast server for testing
#include "beast_test_server.hpp"

#include <boost/log/trivial.hpp>
#include <latch>

namespace net = boost::asio; // from <boost/asio.hpp>
namespace winnet = boost::winasio;

BOOST_AUTO_TEST_SUITE(test_winhttp)

BOOST_AUTO_TEST_CASE(Basic) {
  net::io_context ioc{1};
  std::latch lch{1}; // for waiting for server up
  std::jthread th([&ioc, &lch]() {
    // start beast server
    // auto const address = net::ip::make_address("0.0.0.0");
    unsigned short port = static_cast<unsigned short>(12345);
    myacceptor acceptor{ioc, {tcp::v4(), port}};
    mysocket socket{ioc};
    http_server(acceptor, socket);
    lch.count_down();
    ioc.run();
  });
  // wait for server to run;
  lch.wait();
  // std::this_thread::sleep_for(std::chrono::seconds(1));

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

  // stop server
  ioc.stop();
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
  net::io_context ioc{1};
  std::latch lch{1}; // for waiting for server up
  std::jthread th([&ioc, &lch]() {
    // start beast server
    unsigned short port = static_cast<unsigned short>(12345);
    myacceptor acceptor{ioc, {tcp::v4(), port}};
    mysocket socket{ioc};
    http_server(acceptor, socket);
    lch.count_down();
    ioc.run();
  });
  // wait for server to run;
  lch.wait();

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

  // winnet::winhttp::payload pl;
  // pl.method = L"GET";
  // pl.path = url.get_path();
  // pl.header = std::nullopt;
  // pl.accept = std::nullopt;
  // pl.body = std::nullopt;
  // pl.secure = false;

  winnet::winhttp::basic_winhttp_request_asio_handle<
      net::io_context::executor_type>
      h_request(io_context.get_executor());

  auto f = [&h_request, &h_connect, &url]() -> net::awaitable<void> {
    auto executor = co_await net::this_coro::executor;
    boost::system::error_code ec;
    std::wstring path = url.get_path();
    h_request.managed_open(h_connect.native_handle(), L"GET", path.c_str(),
                           NULL, // http 1.1
                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                           NULL, // no ssl WINHTTP_FLAG_SECURE,
                           ec);
    BOOST_REQUIRE(!ec);

    co_await h_request.async_send(NULL,                    // headers
                                  0,                       // header len
                                  WINHTTP_NO_REQUEST_DATA, // optional
                                  0,                       // optional len
                                  0,                       // total len
                                  net::use_awaitable);

    std::vector<BYTE> body_buff;
    auto dybuff = net::dynamic_buffer(body_buff);

    co_await h_request.async_recieve_response(net::use_awaitable);
    std::size_t len = 0;
    do {
      len = co_await h_request.async_query_data_available(net::use_awaitable);
      auto buff = dybuff.prepare(len + 1);
      std::size_t read_len = co_await h_request.async_read_data(
          (LPVOID)buff.data(), static_cast<DWORD>(len), net::use_awaitable);
      BOOST_REQUIRE_EQUAL(read_len, len);
      dybuff.commit(len);
    } while (len > 0);
    BOOST_TEST_MESSAGE("request body:");
    BOOST_TEST_MESSAGE(winnet::winhttp::buff_to_string(dybuff));
  };

  net::co_spawn(io_context, f, net::detached);

  io_context.run();

  // stop server
  ioc.stop();
}

BOOST_AUTO_TEST_SUITE_END()