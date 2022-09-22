#ifndef BOOST_WINASIO_BASIC_HTTP_URL_HPP
#define BOOST_WINASIO_BASIC_HTTP_URL_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
#pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <boost/assert.hpp>
#include <boost/winasio/http/basic_http_queue.hpp>

namespace boost {
namespace winasio {
namespace http {

template <typename Executor> class basic_http_url {
public:
  typedef Executor executor_type;
  basic_http_url(basic_http_queue<executor_type> &queue_handle,
                 const std::wstring &url)
      : queue_handle_(queue_handle), url_(url) {
    boost::system::error_code ec;
    queue_handle_.add_url(url_, ec);
    BOOST_ASSERT(!ec.failed());
  }

  ~basic_http_url() {
    boost::system::error_code ec;
    queue_handle_.remove_url(url_, ec);
    BOOST_ASSERT(!ec.failed());
  }

  basic_http_url(const basic_http_url &) = delete;
  basic_http_url &operator=(const basic_http_url &) = delete;

  basic_http_url(basic_http_url &&) = delete;
  basic_http_url &operator=(basic_http_url &&) = delete;

private:
  basic_http_queue<executor_type> &queue_handle_;
  std::wstring url_;
};

} // namespace http
} // namespace winasio
} // namespace boost

#endif // BOOST_WINASIO_BASIC_HTTP_URL