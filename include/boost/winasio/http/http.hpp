//
// Copyright (c) 2022 Youyuan Wu
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_WINASIO_HTTP_HPP
#define BOOST_WINASIO_HTTP_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
#pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <boost/asio.hpp>
#include <boost/winasio/http/basic_http_controller.hpp>
#include <boost/winasio/http/basic_http_queue_handle.hpp>
#include <boost/winasio/http/basic_http_request_context.hpp>
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
  DBG_UNREFERENCED_LOCAL_VARIABLE(retCode);
  return hReqQueue;
}

using queue = basic_http_queue_handle<net::any_io_executor>;
using controller = basic_http_controller<net::any_io_executor>;
} // namespace http
} // namespace winasio
} // namespace boost

#endif // BOOST_WINASIO_HTTP_HPP
