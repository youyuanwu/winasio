#pragma once

#include "boost/asio.hpp"
#include <http.h>

#include "boost/winasio/http/basic_http_handle.hpp"

#include "boost/assert.hpp"

#pragma comment(lib, "httpapi.lib")

namespace boost {
namespace winasio {
namespace http {

namespace net = boost::asio;


// simple register and auto remove url
template <typename Executor, HTTP_VERSION http_ver>
class http_simple_url {
public:
  typedef Executor executor_type;
  http_simple_url(basic_http_handle<executor_type> &queue_handle,
                  std::wstring url)
      : queue_handle_(queue_handle), url_(url) {
    boost::system::error_code ec;
    url_handler_.set_queue_handler(queue_handle_.native_handle(), ec);
    BOOST_ASSERT(!ec.failed());
    url_handler_.add_url(url_, ec);
    BOOST_ASSERT(!ec.failed());
  }

  ~http_simple_url() {
    boost::system::error_code ec;
    url_handler_.remove_url(url_, ec);
    BOOST_ASSERT(!ec.failed());
  }

private:
  basic_http_handle<executor_type> &queue_handle_;
  url_handler<http_ver> url_handler_;
  std::wstring  url_;
};

template <HTTP_VERSION http_version> 
class http_initializer {
public:
  http_initializer() { 
      do_init();
  }
  HANDLE get_raw_http_queue() { return req_queue_; }
  ~http_initializer() {
    DWORD retCode =
        HttpTerminate(HTTP_INITIALIZE_SERVER | HTTP_INITIALIZE_CONFIG, NULL);
    BOOST_ASSERT(retCode == NO_ERROR);
  }
private:
  void do_init();
  HANDLE req_queue_ = nullptr;
};

template <> 
void http_initializer<HTTP_VERSION::http_ver_1>::do_init() {
  DWORD retCode =
    HttpInitialize(HTTPAPI_VERSION_1,
                   HTTP_INITIALIZE_SERVER | HTTP_INITIALIZE_CONFIG, // Flags
                   nullptr                                          // Reserved
    );
    BOOST_ASSERT(retCode == NO_ERROR);
    retCode = HttpCreateHttpHandle(&req_queue_, // Req Queue
                                   0            // Reserved
);
BOOST_ASSERT(retCode == NO_ERROR);
}

template <> 
void http_initializer<HTTP_VERSION::http_ver_2>::do_init() {
  DWORD retCode =
      HttpInitialize(HTTPAPI_VERSION_2,
                     HTTP_INITIALIZE_SERVER | HTTP_INITIALIZE_CONFIG, 
                     nullptr);
  BOOST_ASSERT(retCode == NO_ERROR);
  retCode = HttpCreateRequestQueue(HTTPAPI_VERSION_2,
                                    L"Test_Http_Server_HTTPAPI_V2",
                                    nullptr, 0, &req_queue_);
  BOOST_ASSERT(retCode == NO_ERROR);
}
} // namespace http
} // namespace winasio
} // namespace boost