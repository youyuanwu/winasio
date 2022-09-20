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

// open the queue handle
// caller takes ownership
// Note if the request address is not on stack then the request is not routed to
// the server.
inline HANDLE open_raw_http_queue() {
  HANDLE hReqQueue;
  DWORD retCode = HttpCreateHttpHandle(&hReqQueue, // Req Queue
                                       0           // Reserved
  );
  BOOST_ASSERT(retCode == NO_ERROR);
  return hReqQueue;
}

// simple register and auto remove url
template <typename Executor> class http_simple_url {
public:
  typedef Executor executor_type;
  http_simple_url(basic_http_handle<executor_type> &queue_handle,
                  std::wstring url)
      : queue_handle_(queue_handle), url_(url) {
    boost::system::error_code ec;
    queue_handle_.add_url(url_, ec);
    BOOST_ASSERT(!ec.failed());
  }

  ~http_simple_url() {
    boost::system::error_code ec;
    queue_handle_.remove_url(url_, ec);
    BOOST_ASSERT(!ec.failed());
  }

private:
  basic_http_handle<executor_type> &queue_handle_;
  std::wstring url_;
};

class http_initializer {
public:
  http_initializer() {
    DWORD retCode =
        HttpInitialize(HTTPAPI_VERSION_1,
                       HTTP_INITIALIZE_SERVER | HTTP_INITIALIZE_CONFIG, // Flags
                       NULL // Reserved
        );
    BOOST_ASSERT(retCode == NO_ERROR);
  }
  ~http_initializer() {
    DWORD retCode =
        HttpTerminate(HTTP_INITIALIZE_SERVER | HTTP_INITIALIZE_CONFIG, NULL);
    BOOST_ASSERT(retCode == NO_ERROR);
  }
};

} // namespace http
} // namespace winasio
} // namespace boost
