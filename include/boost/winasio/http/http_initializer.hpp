#ifndef BOOST_WINASIO_HTTP_INITIALIZER_HPP
#define BOOST_WINASIO_HTTP_INITIALIZER_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
#pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <boost/winasio/http/http_major_version.hpp>

#include <http.h>

namespace boost {
namespace winasio {
namespace http {

template <HTTP_MAJOR_VERSION> class http_initializer {
public:
  http_initializer() { do_init(); }
  inline HANDLE create_http_queue(boost::system::error_code &ec);

  ~http_initializer() {
    DWORD retCode =
        HttpTerminate(HTTP_INITIALIZE_SERVER | HTTP_INITIALIZE_CONFIG, NULL);
    BOOST_ASSERT(retCode == NO_ERROR);
    DBG_UNREFERENCED_LOCAL_VARIABLE(retCode);
  }

private:
  inline void do_init();
};

template <>
inline void http_initializer<HTTP_MAJOR_VERSION::http_ver_1>::do_init() {
  DWORD retCode =
      HttpInitialize(HTTPAPI_VERSION_1,
                     HTTP_INITIALIZE_SERVER | HTTP_INITIALIZE_CONFIG, // Flags
                     nullptr // Reserved
      );
  BOOST_ASSERT(retCode == NO_ERROR);
  DBG_UNREFERENCED_LOCAL_VARIABLE(retCode);
}

template <>
inline HANDLE
http_initializer<HTTP_MAJOR_VERSION::http_ver_1>::create_http_queue(
    boost::system::error_code &ec) {
  HANDLE req_queue = nullptr;
  DWORD retCode = HttpCreateHttpHandle(&req_queue, // Req Queue
                                       0           // Reserved
  );
  ec = system::error_code(retCode, asio::error::get_system_category());
  return req_queue;
}

template <>
inline void http_initializer<HTTP_MAJOR_VERSION::http_ver_2>::do_init() {
  DWORD retCode =
      HttpInitialize(HTTPAPI_VERSION_2,
                     HTTP_INITIALIZE_SERVER | HTTP_INITIALIZE_CONFIG, nullptr);
  BOOST_ASSERT(retCode == NO_ERROR);
  DBG_UNREFERENCED_LOCAL_VARIABLE(retCode);
}

template <>
inline HANDLE
http_initializer<HTTP_MAJOR_VERSION::http_ver_2>::create_http_queue(
    boost::system::error_code &ec) {
  HANDLE req_queue = nullptr;
  DWORD retCode =
      HttpCreateRequestQueue(HTTPAPI_VERSION_2, L"Test_Http_Server_HTTPAPI_V2",
                             nullptr, 0, &req_queue);

  ec = system::error_code(retCode, asio::error::get_system_category());
  return req_queue;
}

} // namespace http
} // namespace winasio
} // namespace boost

#endif // BOOST_WINASIO_HTTP_INITIALIZER_HPP
