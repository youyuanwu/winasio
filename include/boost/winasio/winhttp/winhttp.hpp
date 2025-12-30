//
// Copyright (c) 2022 Youyuan Wu
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/windows/basic_object_handle.hpp>

#include "boost/assert.hpp"

#ifdef WINASIO_LOG
#include <spdlog/spdlog.h>
#endif

#include "winhttp.h"

#include <array>
#include <optional>
#include <string>

#pragma comment(lib, "winhttp.lib")

namespace boost {
namespace winasio {
namespace winhttp {
namespace net = boost::asio;
// namespace winnet = net::windows;

class url_component {
public:
  url_component() : uc_() {
    uc_.dwStructSize = sizeof(uc_);
    uc_.lpszHostName = szHost_.data();
    uc_.dwHostNameLength = std::tuple_size<decltype(szHost_)>::value;
    uc_.lpszUrlPath = szPath_.data();
    uc_.dwUrlPathLength = std::tuple_size<decltype(szPath_)>::value;
    uc_.lpszScheme = szScheme_.data();
    uc_.dwSchemeLength = std::tuple_size<decltype(szScheme_)>::value;
    uc_.lpszExtraInfo = szQuery_.data();
    uc_.dwExtraInfoLength = std::tuple_size<decltype(szQuery_)>::value;
  }

  inline void crack(LPCWSTR pwszUrl, DWORD dwUrlLength, DWORD dwFlags,
                    boost::system::error_code &ec) {
    bool ok = WinHttpCrackUrl(pwszUrl, dwUrlLength, dwFlags, &uc_);
    if (!ok) {
      ec = boost::system::error_code(GetLastError(),
                                     boost::asio::error::get_system_category());
    }
  }

  inline void crack(std::wstring const &url, boost::system::error_code &ec) {
    return crack(url.c_str(), static_cast<DWORD>(url.size()), 0, ec);
  }

  inline const std::wstring get_hostname() {
    return std::wstring(uc_.lpszHostName, uc_.dwHostNameLength);
  }

  inline const std::wstring get_scheme() {
    return std::wstring(uc_.lpszScheme, uc_.dwSchemeLength);
  }

  inline const std::wstring get_path() {
    return std::wstring(uc_.lpszUrlPath, uc_.dwUrlPathLength);
  }
  inline const std::wstring get_query() {
    return std::wstring(uc_.lpszExtraInfo, uc_.dwExtraInfoLength);
  }
  inline const INTERNET_SCHEME get_nscheme() { return uc_.nScheme; }
  inline const INTERNET_PORT get_port() { return uc_.nPort; }

  inline const URL_COMPONENTS &get() { return uc_; }

private:
  URL_COMPONENTS uc_;
  std::array<WCHAR, 128> szHost_;
  std::array<WCHAR, 128> szPath_;
  std::array<WCHAR, 64> szScheme_;
  std::array<WCHAR, 128> szQuery_;
};

template <typename Executor = net::any_io_executor> class basic_winhttp_handle {
public:
  /// The type of the executor associated with the object.
  typedef Executor executor_type;

  explicit basic_winhttp_handle(const executor_type &ex)
      : handle_(nullptr), ex_(ex) {}

  template <typename ExecutionContext>
  explicit basic_winhttp_handle(
      ExecutionContext &context,
      typename net::constraint<
          net::is_convertible<ExecutionContext &,
                              net::execution_context &>::value,
          net::defaulted_constraint>::type = net::defaulted_constraint())
      : handle_(nullptr), ex_(context.get_executor()) {}

  // basic_winhttp_handle(HINTERNET handle): handle_(handle) {}

  void assign(HINTERNET handle) {
    if (handle_ != nullptr) {
      this->close();
    }
    handle_ = handle;
  }

  void close() {
    if (handle_ != nullptr) {
      bool ok = WinHttpCloseHandle(handle_);
      if (!ok) {
        boost::system::error_code ec = boost::system::error_code(
            GetLastError(), boost::asio::error::get_system_category());
        BOOST_ASSERT(!ec.failed());
      }
    }
    handle_ = nullptr;
  }

  ~basic_winhttp_handle() { this->close(); }

  HINTERNET native_handle() { return handle_; }

  WINHTTP_STATUS_CALLBACK
  set_status_callback(_In_ WINHTTP_STATUS_CALLBACK lpfnInternetCallback,
                      _In_ DWORD dwNotificationFlags,
                      _Inout_ boost::system::error_code &ec) {
    WINHTTP_STATUS_CALLBACK prevCallback = WinHttpSetStatusCallback(
        this->native_handle(), lpfnInternetCallback, dwNotificationFlags,
        NULL // reserved
    );
    if (prevCallback == WINHTTP_INVALID_STATUS_CALLBACK) {
      ec = boost::system::error_code(GetLastError(),
                                     boost::asio::error::get_system_category());
    }
#ifdef WINASIO_LOG
    spdlog::debug("WinHttpSetStatusCallback: status: {}", ec.message());
#endif
    return prevCallback;
  }

  WINHTTP_STATUS_CALLBACK
  set_status_callback(_In_ WINHTTP_STATUS_CALLBACK lpfnInternetCallback,
                      _Inout_ boost::system::error_code &ec) {
    return set_status_callback(lpfnInternetCallback,
                               WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS, ec);
  }

  // set option
  void set_option(_In_ DWORD dwOption, _In_ LPVOID lpBuffer,
                  _In_ DWORD dwBufferLength,
                  _Out_ boost::system::error_code &ec) {
    bool ok = WinHttpSetOption(this->native_handle(), dwOption, lpBuffer,
                               dwBufferLength);
    if (!ok) {
      ec = boost::system::error_code(GetLastError(),
                                     boost::asio::error::get_system_category());
    }
#ifdef WINASIO_LOG
    spdlog::debug("WinHttpSetOption: status: {}", ec.message());
#endif
  }

  executor_type get_executor() { return ex_; }

private:
  HINTERNET handle_;
  executor_type ex_;
};

template <typename Executor = net::any_io_executor>
class basic_winhttp_session_handle : public basic_winhttp_handle<Executor> {
public:
  /// The type of the executor associated with the object.
  typedef Executor executor_type;

  typedef basic_winhttp_handle<executor_type> parent_type;

  explicit basic_winhttp_session_handle(const executor_type &ex)
      : basic_winhttp_handle(ex) {}

  template <typename ExecutionContext>
  explicit basic_winhttp_session_handle(
      ExecutionContext &context,
      typename net::constraint<
          net::is_convertible<ExecutionContext &,
                              net::execution_context &>::value,
          net::defaulted_constraint>::type = net::defaulted_constraint())
      : basic_winhttp_handle<executor_type>(context) {}

  void open(LPCWSTR agent, DWORD access_type, LPCWSTR proxy_name,
            LPCWSTR proxy_bypass, DWORD flags, boost::system::error_code &ec) {
    HINTERNET hSession =
        WinHttpOpen(agent, access_type, proxy_name, proxy_bypass, flags);
    if (!hSession) {
      ec = boost::system::error_code(GetLastError(),
                                     boost::asio::error::get_system_category());
    } else {
      parent_type::assign(hSession);
    }
#ifdef WINASIO_LOG
    spdlog::debug("WinHttpOpen: status: {}", ec.message());
#endif
  }

  // most basic open
  void open(boost::system::error_code &ec) {
    open(L"WinASIO", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
         WINHTTP_NO_PROXY_BYPASS,
         WINHTTP_FLAG_ASYNC, // by default it is async
         ec);
  }
};

template <typename Executor = net::any_io_executor>
class basic_winhttp_connect_handle : public basic_winhttp_handle<Executor> {
public:
  /// The type of the executor associated with the object.
  typedef Executor executor_type;

  typedef basic_winhttp_handle<executor_type> parent_type;

  explicit basic_winhttp_connect_handle(const executor_type &ex)
      : basic_winhttp_handle(ex) {}

  template <typename ExecutionContext>
  explicit basic_winhttp_connect_handle(
      ExecutionContext &context,
      typename net::constraint<
          net::is_convertible<ExecutionContext &,
                              net::execution_context &>::value,
          net::defaulted_constraint>::type = net::defaulted_constraint())
      : basic_winhttp_handle<executor_type>(context) {}

  // connect is always synchronous.
  void connect(HINTERNET h_session, LPCWSTR url, INTERNET_PORT port,
               boost::system::error_code &ec) {
    HINTERNET hConnect = WinHttpConnect(h_session, url, port, 0 // reserved
    );
    if (!hConnect) {
      ec = boost::system::error_code(GetLastError(),
                                     boost::asio::error::get_system_category());
    } else {
      parent_type::assign(hConnect);
    }
#ifdef WINASIO_LOG
    spdlog::debug(L"WinHttpConnect: URL={} port={}", url, port);
    if (ec) {
      spdlog::debug("WinHttpConnect: status: {}", ec.message());
    }
#endif
  }
};

template <typename Executor = net::any_io_executor>
class basic_winhttp_request_handle : public basic_winhttp_handle<Executor> {
public:
  /// The type of the executor associated with the object.
  typedef Executor executor_type;

  typedef basic_winhttp_handle<executor_type> parent_type;

  explicit basic_winhttp_request_handle(const executor_type &ex)
      : basic_winhttp_handle<executor_type>(ex) {}

  template <typename ExecutionContext>
  explicit basic_winhttp_request_handle(
      ExecutionContext &context,
      typename net::constraint<
          net::is_convertible<ExecutionContext &,
                              net::execution_context &>::value,
          net::defaulted_constraint>::type = net::defaulted_constraint())
      : basic_winhttp_handle<executor_type>(context) {}

  // open is always synchronous
  void open(HINTERNET hConnect, LPCWSTR pwszVerb, LPCWSTR pwszObjectName,
            LPCWSTR pwszVersion, LPCWSTR pwszReferrer,
            LPCWSTR *ppwszAcceptTypes, DWORD dwFlags,
            boost::system::error_code &ec) {
    HINTERNET hRequest =
        WinHttpOpenRequest(hConnect, pwszVerb, pwszObjectName, pwszVersion,
                           pwszReferrer, ppwszAcceptTypes, dwFlags);
    if (!hRequest) {
      ec = boost::system::error_code(GetLastError(),
                                     boost::asio::error::get_system_category());
    } else {
      parent_type::assign(hRequest);
    }
#ifdef WINASIO_LOG
    spdlog::debug("WinHttpOpenRequest: status: {}", ec.message());
#endif
  }

  void open(HINTERNET hConnect, LPCWSTR method, LPCWSTR path,
            boost::system::error_code &ec) {
    open(hConnect, method, path,
         NULL, // http 1.1
         WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE,
         ec);
  }

  // send request. post to asio executor to wait for completion
  // winhttp callback enum is WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE
  void send(LPCWSTR lpszHeaders, DWORD dwHeadersLength, LPVOID lpOptional,
            DWORD dwOptionalLength, DWORD dwTotalLength, DWORD_PTR dwContext,
            boost::system::error_code &ec) {
    bool ok = WinHttpSendRequest(parent_type::native_handle(), lpszHeaders,
                                 dwHeadersLength, lpOptional, dwOptionalLength,
                                 dwTotalLength, dwContext);
    if (!ok) {
      ec = boost::system::error_code(GetLastError(),
                                     boost::asio::error::get_system_category());
    }
#ifdef WINASIO_LOG
    spdlog::debug("WinHttpSendRequest: status: {}", ec.message());
#endif
  }

  void query_headers(_In_ DWORD dwInfoLevel, _In_opt_ LPCWSTR pwszName,
                     _Out_ LPVOID lpBuffer, _Inout_ LPDWORD lpdwBufferLength,
                     _Inout_ LPDWORD lpdwIndex,
                     _Inout_ boost::system::error_code &ec) {
    bool ok =
        WinHttpQueryHeaders(parent_type::native_handle(), dwInfoLevel, pwszName,
                            lpBuffer, lpdwBufferLength, lpdwIndex);
    if (!ok) {
      ec = boost::system::error_code(GetLastError(),
                                     boost::asio::error::get_system_category());
    }
#ifdef WINASIO_LOG
    spdlog::debug("WinHttpQueryHeaders: status: {}", ec.message());
#endif
  }

  // callback case WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE
  void query_data_available(_Out_ LPDWORD lpdwNumberOfBytesAvailable,
                            _Inout_ boost::system::error_code &ec) {
    bool ok = WinHttpQueryDataAvailable(parent_type::native_handle(),
                                        lpdwNumberOfBytesAvailable);
    if (!ok) {
      ec = boost::system::error_code(GetLastError(),
                                     boost::asio::error::get_system_category());
    }
#ifdef WINASIO_LOG
    spdlog::debug("WinHttpQueryDataAvailable: status: {}", ec.message());
#endif
  }

  // callback case: WINHTTP_CALLBACK_STATUS_READ_COMPLETE
  void read_data(_Out_ LPVOID lpBuffer, _In_ DWORD dwNumberOfBytesToRead,
                 _Out_ LPDWORD lpdwNumberOfBytesRead,
                 _Inout_ boost::system::error_code &ec) {
    bool ok = WinHttpReadData(parent_type::native_handle(), lpBuffer,
                              dwNumberOfBytesToRead, lpdwNumberOfBytesRead);
    if (!ok) {
      ec = boost::system::error_code(GetLastError(),
                                     boost::asio::error::get_system_category());
    }
#ifdef WINASIO_LOG
    spdlog::debug("WinHttpReadData: ToRead= {} status: {}",
                  dwNumberOfBytesToRead, ec.message());
#endif
  }

  void write_data(_In_ LPCVOID lpBuffer, _In_ DWORD dwNumberOfBytesToWrite,
                  _Out_ LPDWORD lpdwNumberOfBytesWritten,
                  _Out_ boost::system::error_code &ec) {
    bool ok =
        WinHttpWriteData(parent_type::native_handle(), lpBuffer,
                         dwNumberOfBytesToWrite, lpdwNumberOfBytesWritten);
    if (!ok) {
      ec = boost::system::error_code(GetLastError(),
                                     boost::asio::error::get_system_category());
    }
#ifdef WINASIO_LOG
    spdlog::debug("WinHttpWriteData: ToWrite= {} status: {}",
                  dwNumberOfBytesToWrite, ec.message());
#endif
  }

  // call back case: WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE
  void receive_response(_Out_ boost::system::error_code &ec) {
    bool ok = WinHttpReceiveResponse(parent_type::native_handle(), NULL);
    if (!ok) {
      ec = boost::system::error_code(GetLastError(),
                                     boost::asio::error::get_system_category());
    }
#ifdef WINASIO_LOG
    spdlog::debug("WinHttpReceiveResponse: status: {}", ec.message());
#endif
  }

  void add_headers(_In_ LPCWSTR lpszHeaders, _In_ DWORD dwHeadersLength,
                   _In_ DWORD dwModifiers,
                   _Out_ boost::system::error_code &ec) {
    bool ok =
        WinHttpAddRequestHeaders(parent_type::native_handle(), lpszHeaders,
                                 dwHeadersLength, dwModifiers);
    if (!ok) {
      ec = boost::system::error_code(GetLastError(),
                                     boost::asio::error::get_system_category());
    }
#ifdef WINASIO_LOG
    spdlog::debug("WinHttpAddRequestHeaders: status: {}", ec.message());
#endif
  }
};

namespace header {

template <typename Executor = net::any_io_executor>
void get_status_code(basic_winhttp_request_handle<Executor> &h,
                     _Out_ boost::system::error_code &ec,
                     _Out_ DWORD &dwStatusCode) {
  DWORD dwSize = sizeof(dwStatusCode);
  // read status code as a number
  h.query_headers(WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                  WINHTTP_HEADER_NAME_BY_INDEX, &dwStatusCode, &dwSize,
                  WINHTTP_NO_HEADER_INDEX, ec);
}

// todo: make private
template <typename Executor = net::any_io_executor>
void get_string_header_helper(basic_winhttp_request_handle<Executor> &h,
                              _In_ DWORD dwInfoLevel,
                              _Out_ boost::system::error_code &ec,
                              _Out_ std::wstring &data) {
  DWORD dwSize;
  // read version length
  h.query_headers(dwInfoLevel, WINHTTP_HEADER_NAME_BY_INDEX, NULL, &dwSize,
                  WINHTTP_NO_HEADER_INDEX, ec);
  if (ec.value() != ERROR_INSUFFICIENT_BUFFER) {
    return;
  }
  ec.clear();
  data.resize(dwSize);
  h.query_headers(dwInfoLevel, WINHTTP_HEADER_NAME_BY_INDEX, data.data(),
                  &dwSize, WINHTTP_NO_HEADER_INDEX, ec);
  // the buffer requested by winhttp might be too large and filled with nulls at
  // the end. erase tail nulls.
  data.erase(std::find(data.begin(), data.end(), '\0'), data.end());
}

template <typename Executor = net::any_io_executor>
void get_version(basic_winhttp_request_handle<Executor> &h,
                 _Out_ boost::system::error_code &ec,
                 _Out_ std::wstring &data) {
  get_string_header_helper(h, WINHTTP_QUERY_VERSION, ec, data);
}

template <typename Executor = net::any_io_executor>
void get_content_type(basic_winhttp_request_handle<Executor> &h,
                      _Out_ boost::system::error_code &ec,
                      _Out_ std::wstring &data) {
  get_string_header_helper(h, WINHTTP_QUERY_CONTENT_TYPE, ec, data);
}

template <typename Executor = net::any_io_executor>
void get_all_raw_crlf(basic_winhttp_request_handle<Executor> &h,
                      _Out_ boost::system::error_code &ec,
                      _Out_ std::wstring &data) {
  get_string_header_helper(h, WINHTTP_QUERY_RAW_HEADERS_CRLF, ec, data);
}

template <typename Executor = net::any_io_executor>
void get_trailers(basic_winhttp_request_handle<Executor> &h,
                  _Out_ boost::system::error_code &ec,
                  _Out_ std::wstring &data) {
  // Trailer has to be queried together with crlf
  // only trailer will be returned.
  // See:
  // https://github.com/dotnet/runtime/blob/4cbe6f99d23e04c56a89251d49de1b0f14000427/src/libraries/System.Net.Http.WinHttpHandler/src/System/Net/Http/WinHttpResponseParser.cs#L230
  get_string_header_helper(
      h, WINHTTP_QUERY_FLAG_TRAILERS | WINHTTP_QUERY_RAW_HEADERS_CRLF, ec,
      data);
}

// add a single header. multiple might also work
template <typename Executor = net::any_io_executor>
void add_header(basic_winhttp_request_handle<Executor> &h,
                _In_ std::wstring &data, _Out_ boost::system::error_code &ec) {
  h.add_headers(data.c_str(), data.length(), ec);
}

// accept types
// const wchar_t *att[] = { L"application/json", NULL };
// argument for WinHttpOpenRequest accept type arg
class accept_types {
public:
  accept_types(std::initializer_list<std::wstring> l) : data_(l) {}

  inline const LPCWSTR *get() {
    if (data_.size() == 0) {
      return nullptr;
    }
    for (auto &str : data_) {
      output_.push_back(str.c_str());
    }
    output_.push_back(nullptr); // end of array
    return output_.data();
  }

private:
  std::vector<const wchar_t *> output_;
  std::vector<std::wstring> data_;
};

// argument for
class headers {
public:
  headers() : data_() {}
  headers &add(std::wstring key, std::wstring val) {
    // linebreak not needed for the last element
    if (!data_.empty()) {
      data_.append(L"\r\n");
    }
    data_.append(key).append(L": ").append(val);
    return *this;
  }
  const LPCWSTR get() const {
    if (data_.empty()) {
      return WINHTTP_NO_ADDITIONAL_HEADERS;
    }
    return data_.data();
  }

  DWORD size() const { return static_cast<DWORD>(data_.size()); }

private:
  std::wstring data_;
};

} // namespace header

namespace error {

// get failure code string for callback case
// WINHTTP_CALLBACK_STATUS_SECURE_FAILURE
inline void get_secure_failure_err_str(_In_ DWORD code,
                                       _Out_ std::wstring &data) {
  /*If the dwInternetStatus parameter is
       WINHTTP_CALLBACK_STATUS_SECURE_FAILURE, this parameter can be a
       bitwise-OR combination of one or more of the following values:
            WINHTTP_CALLBACK_STATUS_FLAG_CERT_REV_FAILED
            Certification revocation checking has been enabled, but the
       revocation check failed to verify whether a certificate has been
       revoked.The server used to check for revocation might be unreachable.
            WINHTTP_CALLBACK_STATUS_FLAG_INVALID_CERT
            SSL certificate is invalid.
            WINHTTP_CALLBACK_STATUS_FLAG_CERT_REVOKED
            SSL certificate was revoked.
            WINHTTP_CALLBACK_STATUS_FLAG_INVALID_CA
            The function is unfamiliar with the Certificate Authority that
       generated the server's certificate.
            WINHTTP_CALLBACK_STATUS_FLAG_CERT_CN_INVALID
            SSL certificate common name(host name field) is incorrect, for
       example, if you entered www.microsoft.com and the common name on the
       certificate says www.msn.com.
            WINHTTP_CALLBACK_STATUS_FLAG_CERT_DATE_INVALID
            SSL certificate date that was received from the server is bad.The
       certificate is expired.
            WINHTTP_CALLBACK_STATUS_FLAG_SECURITY_CHANNEL_ERROR
            The application experienced an internal error loading the SSL
       libraries.
    */
  std::wstringstream error_ss;
  error_ss << L"SECURE_FAILURE " << code;
  if (code & WINHTTP_CALLBACK_STATUS_FLAG_CERT_REV_FAILED) // 1
  {
    error_ss << L"Revocation check failed to verify whether a "
                L"certificate has been revoked.";
  }
  if (code & WINHTTP_CALLBACK_STATUS_FLAG_INVALID_CERT) // 2
  {
    error_ss << L"SSL certificate is invalid.";
  }
  if (code & WINHTTP_CALLBACK_STATUS_FLAG_CERT_REVOKED) // 4
  {
    error_ss << L"SSL certificate was revoked.";
  }
  if (code & WINHTTP_CALLBACK_STATUS_FLAG_INVALID_CA) // 8
  {
    error_ss << L"The function is unfamiliar with the Certificate Authority "
                L"that generated the server\'s certificate.";
  }
  if (code & WINHTTP_CALLBACK_STATUS_FLAG_CERT_CN_INVALID) // 10
  {
    error_ss << L"SSL certificate common name(host name field) is incorrect";
  }
  if (code & WINHTTP_CALLBACK_STATUS_FLAG_CERT_DATE_INVALID) // 20
  {
    error_ss << L"CSSL certificate date that was received from the "
                L"server is bad.The certificate is expired.";
  }
  if (code & WINHTTP_CALLBACK_STATUS_FLAG_SECURITY_CHANNEL_ERROR) // 80000000
  {
    error_ss << L"The application experienced an internal error "
                L"loading the SSL libraries.";
  }
  data = error_ss.str();
}

namespace detail {
// This macro returns the constant name in a string.
#define CASE_OF(constant)                                                      \
  case constant:                                                               \
    return (L#constant)

inline LPCWSTR get_api_error_result_str(DWORD dwResult) {
  // Return the error result as a string so that the
  // name of the function causing the error can be displayed.
  switch (dwResult) {
    CASE_OF(API_RECEIVE_RESPONSE);
    CASE_OF(API_QUERY_DATA_AVAILABLE);
    CASE_OF(API_READ_DATA);
    CASE_OF(API_WRITE_DATA);
    CASE_OF(API_SEND_REQUEST);
  }
  return L"Unknown function";
}
} // namespace detail

// return the error string from WINHTTP_ASYNC_RESULT
// from async case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR
inline void get_api_error_str(_In_ WINHTTP_ASYNC_RESULT *pAR,
                              _Out_ std::wstring &data) {
  std::wstringstream error_ss;
  error_ss << "REQUEST_ERROR - error " << pAR->dwError << ", result "
           << detail::get_api_error_result_str(
                  static_cast<DWORD>(pAR->dwResult));
  data = error_ss.str();
}

} // namespace error

} // namespace winhttp
} // namespace winasio
} // namespace boost
