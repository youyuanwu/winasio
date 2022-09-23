#ifndef BOOST_WINASIO_HTTP_MAJOR_VERSION_HPP
#define BOOST_WINASIO_HTTP_MAJOR_VERSION_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
#pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <http.h>

namespace boost {
namespace winasio {
namespace http {

constexpr HTTPAPI_VERSION Http_Api_Version_1 = HTTPAPI_VERSION_1;
constexpr HTTPAPI_VERSION Http_Api_Version_2 = HTTPAPI_VERSION_2;

enum class HTTP_MAJOR_VERSION {
  http_ver_1 = Http_Api_Version_1.HttpApiMajorVersion,
  http_ver_2 = Http_Api_Version_2.HttpApiMajorVersion
};

} // namespace http
} // namespace winasio
} // namespace boost

#endif // BOOST_WINASIO_HTTP_MAJOR_VERSION_HPP
