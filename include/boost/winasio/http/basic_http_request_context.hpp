#ifndef BOOST_WINASIO_HTTP_REQUEST_CONTEXT_HPP
#define BOOST_WINASIO_HTTP_REQUEST_CONTEXT_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
#pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

namespace boost {
namespace winasio {
namespace http {

template <typename RequestT, typename ResponseT>
struct basic_http_request_context {

  const RequestT request;
  ResponseT response;
};
} // namespace http
} // namespace winasio
} // namespace boost

#endif // BOOST_WINASIO_HTTP_REQUEST_CONTEXT_HPP
