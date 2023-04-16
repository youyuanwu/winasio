//
// Copyright (c) 2022 Youyuan Wu
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

#include "boost/winasio/winhttp/winhttp.hpp"
#include "boost/winasio/winhttp/winhttp_asio.hpp"

namespace boost {
namespace winasio {
namespace winhttp {

namespace net = boost::asio; // from <boost/asio.hpp>
namespace winnet = boost::winasio;

class payload {
public:
  bool secure;
  std::wstring method;
  std::optional<std::wstring> path;
  std::optional<header::accept_types> accept;
  std::optional<header::headers> header;
  std::optional<std::string> body;
  bool insecure_skip_verify;
};

namespace details {

// compose operation that reads all http body into buff
template <typename Executor, typename DynamicBuffer>
class async_exec_op : boost::asio::coroutine {
public:
  typedef Executor executor_type;
  async_exec_op(basic_winhttp_request_asio_handle<executor_type> &h,
                DynamicBuffer &buff, LPCWSTR lpszHeaders, DWORD dwHeadersLength,
                LPVOID lpOptional, DWORD dwOptionalLength, DWORD dwTotalLength)
      : h_(h), buff_(buff), lpszHeaders_(lpszHeaders),
        dwHeadersLength_(dwHeadersLength), lpOptional_(lpOptional),
        dwOptionalLength_(dwOptionalLength), dwTotalLength_(dwTotalLength),
        state_(state::idle) {}

  template <typename Self>
  void operator()(Self &self, boost::system::error_code ec = {},
                  std::size_t len = 0) {
    if (ec) {
      self.complete(ec, len);
      return;
    }

    switch (state_) {
    case state::idle:
      state_ = state::send;
      h_.async_send(lpszHeaders_, dwHeadersLength_, lpOptional_,
                    dwOptionalLength_, dwTotalLength_, std::move(self));
      break;
    case state::send:
      state_ = state::receive_response;
      h_.async_recieve_response(std::move(self));
      break;
    case state::receive_response:
      // read body and finish
      state_ = state::done;
      async_read_body(h_, buff_, std::move(self));
      break;
    case state::done:
      self.complete(ec, 0);
      break;
    default:
      BOOST_ASSERT_MSG(false, "unknown state");
      break;
    }
  }

private:
  basic_winhttp_request_asio_handle<executor_type> &h_;
  DynamicBuffer &buff_;
  LPCWSTR lpszHeaders_;
  DWORD dwHeadersLength_;
  LPVOID lpOptional_;
  DWORD dwOptionalLength_;
  DWORD dwTotalLength_;
  enum class state { idle, send, receive_response, done } state_;
};
} // namespace details

template <typename Executor, typename DynamicBuffer,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::system::error_code,
                                               std::size_t))
              Token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(Executor)>
void async_exec(payload &p, basic_winhttp_connect_handle<Executor> &h_connect,
                basic_winhttp_request_asio_handle<Executor> &h_request,
                DynamicBuffer &buffer, Token &&token) {
  boost::system::error_code ec;

  LPCWSTR ppath = NULL;
  if (p.path) {
    ppath = p.path->c_str();
  }
  LPCWSTR *p_accept = NULL;
  if (p.accept) {
    p_accept = (LPCWSTR *)p.accept->get();
  }
  DWORD flags = 0;
  if (p.secure) {
    flags |= WINHTTP_FLAG_SECURE;
  }

  // open request
  h_request.managed_open(h_connect.native_handle(), p.method.c_str(), ppath,
                         NULL, // http 1.1
                         WINHTTP_NO_REFERER, p_accept, flags, ec);
  if (ec) {
    net::post(h_request.get_executor(), std::bind(token, ec, 0));
    return;
  }

  // skip cert check
  if (p.secure && p.insecure_skip_verify) {
    // disable all sec checks
    // may need to revert this
    DWORD dwSecurityFlags = SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
                            SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                            SECURITY_FLAG_IGNORE_UNKNOWN_CA |
                            SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
    h_request.set_option(WINHTTP_OPTION_SECURITY_FLAGS, (PVOID)&dwSecurityFlags,
                         sizeof(dwSecurityFlags), ec);
    if (ec) {
#ifdef WINASIO_LOG
      BOOST_LOG_TRIVIAL(debug) << L"set_option for security failed: " << ec;
#endif
      net::post(h_request.get_executor(), std::bind(token, ec, 0));
      return;
    }
  }

  LPCWSTR lpszHeaders = NULL;
  DWORD dwHeadersLength = 0;
  if (p.header) {
    lpszHeaders = p.header->get();
    dwHeadersLength = static_cast<DWORD>(p.header->size());
  }

  LPVOID lpOptional = WINHTTP_NO_REQUEST_DATA;
  DWORD dwOptionalLength = 0;
  if (p.body) {
    lpOptional = (LPVOID)p.body->data();
    dwOptionalLength = static_cast<DWORD>(p.body->size());
  }
  DWORD dwTotalLength = dwOptionalLength; // the same for now.

  // send request
  return boost::asio::async_compose<Token, void(boost::system::error_code,
                                                std::size_t)>(
      details::async_exec_op<Executor, DynamicBuffer>(
          h_request, buffer, lpszHeaders, dwHeadersLength, lpOptional,
          dwOptionalLength, dwTotalLength),
      token, h_request.get_executor());
}

// helper to convert dynamic buff to string
template <typename DynamicBuffer>
std::string buff_to_string(DynamicBuffer &buff) {
  auto resp_body = buff.data();
  auto view = resp_body.data();
  auto size = resp_body.size();
  return std::string((BYTE *)view, (BYTE *)view + size);
}

} // namespace winhttp
} // namespace winasio
} // namespace boost