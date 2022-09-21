#ifndef BOOST_WINASIO_HTTP_HPP
#define BOOST_WINASIO_HTTP_HPP

#include <boost/asio.hpp>
#include <boost/winasio/http/basic_http_controller.hpp>
#include <boost/winasio/http/basic_http_queue.hpp>
#include <boost/winasio/http/basic_http_url.hpp>

#include <boost/winasio/http/convert.hpp>
#include <boost/winasio/http/http_asio.hpp>
#include <boost/winasio/http/http_initializer.hpp>

#include <boost/assert.hpp>

#include <http.h>
#pragma comment(lib, "httpapi.lib")

namespace boost {
namespace winasio {
namespace http {

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

using queue = basic_http_queue<net::io_context::executor_type>;
using controller = basic_http_controller<net::io_context::executor_type>;
namespace v1 {
using url = basic_http_url<net::io_context::executor_type>;
} // namespace v1
} // namespace http
} // namespace winasio
} // namespace boost

#endif // BOOST_WINASIO_HTTP_HPP