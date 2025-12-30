//
// Copyright (c) 2022 Youyuan Wu
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

// aims to have asio style apis
// send_header(handle, token)
#include "boost/winasio/winhttp/winhttp.hpp"
#include <functional>
#include <spdlog/spdlog.h>

namespace boost {
namespace winasio {
namespace winhttp {

namespace net = boost::asio; // from <boost/asio.hpp>
namespace winnet = boost::winasio;

namespace details {

// Compose operation to support coroutine
// pass in an oh, and init by async compose, in WinHttp callback then set the
// event, then the operation is compeleted. This is just a wrapper to support
// coroutine return type.
// The idea is to pass ec and len in ctx and access them here and pass to user
// handler. The ctx in the asio_winhttp handle should out last the async
// operations, so the pointers passed in ctor should be valid in the life time.
//
// The intended handler type is void (boost::system::error_code ec)
template <typename Executor> class async_op : boost::asio::coroutine {
public:
  // oh needs to persist until async operation finishes.
  async_op(net::windows::basic_object_handle<Executor> *oh,
           boost::system::error_code *ctx_ec)
      : oh_(oh), ctx_ec_(ctx_ec) {}

  template <typename Self>
  void operator()(Self &self, boost::system::error_code ec = {}) {
    if (ec.failed()) {
      // trigger user handler
      self.complete(ec);
      return;
    }

    oh_->async_wait([self = std::move(self),
                     c = ctx_ec_](boost::system::error_code ec) mutable {
      // we don't expect event can fail.
      assert(!ec.failed());
      DBG_UNREFERENCED_LOCAL_VARIABLE(ec);
      // This invokes the user handler.
      // pass the ctx_ec to the handler.
      self.complete(*c);
    });
  }

private:
  net::windows::basic_object_handle<Executor> *oh_;
  boost::system::error_code *ctx_ec_;
};

// differ from the above by having the len signature.
// used to complete handler with len.
// The intended handler type is void (boost::system::error_code ec, std::size_t
// len)
template <typename Executor> class async_len_op : boost::asio::coroutine {
public:
  // oh needs to persist until async operation finishes.
  async_len_op(net::windows::basic_object_handle<Executor> *oh,
               boost::system::error_code *ctx_ec, std::size_t *len)
      : oh_(oh), ctx_ec_(ctx_ec), len_(len) {}

  template <typename Self>
  void operator()(Self &self, boost::system::error_code ec = {}) {
    if (ec.failed()) {
      // trigger user handler
      self.complete(ec, 0);
      return;
    }
    // len_ and ctx_ec_ are owned by request hanle where it is guaranteed to be
    // valid when operation is going on. capture this causes problems because
    // this can be gone when async_compose frontend finishes.
    oh_->async_wait([self = std::move(self), l = len_,
                     c = ctx_ec_](boost::system::error_code ec) mutable {
      assert(!ec.failed());
      DBG_UNREFERENCED_LOCAL_VARIABLE(ec);
      // pass the ctx_ec and len to the handler
      self.complete(*c, *l);
    });
  }

private:
  net::windows::basic_object_handle<Executor> *oh_;
  boost::system::error_code *ctx_ec_;
  std::size_t *len_;
};

// The hook between asio and winhttp is through a single object handle (event).
// where user handler's are moved into the object handle, when winhttp frontend
// api is triggered, and winhttp backend thread (callback) sets the event, so
// that the user handler gets invoked. In each step/winhttp front end
// invokation, the event is reset, and then reused.
template <typename Executor> class asio_request_context {
public:
  typedef Executor executor_type;
  enum class state {
    idle,
    send_request,
    headers_available,
    data_available,
    read_complete,
    write_complete,
    error
  };

  // callbacks
  asio_request_context(const executor_type &ex)
      : ex_(ex), step_event(ex), step_ec(), step_len(0), state_(state::idle) {
    HANDLE ev = CreateEvent(NULL,  // default security attributes
                            TRUE,  // manual-reset event
                            FALSE, // initial state is nonsignaled
                            NULL   // object name
    );
    assert(ev != nullptr);
    this->step_event.assign(ev);
  }

  void set_state(state s) noexcept { state_ = s; }

  state get_state() const noexcept { return state_; }

  // completes the current step.
  // This sets the event so that asio handler/token gets run,
  // and ec value will be accessable in handler.
  void step_complete(boost::system::error_code ec, std::size_t len = 0) {
    spdlog::debug("step_complete: state={} ec={} len={}",
                  static_cast<int>(state_), ec.message(), len);
    // remembers the error and then set the event
    this->step_ec = ec;
    this->step_len = len;

    HANDLE h = this->step_event.native_handle();
    assert(h != NULL);
    assert(h != INVALID_HANDLE_VALUE);
    bool ok = SetEvent(h);
    assert(ok);
    DBG_UNREFERENCED_LOCAL_VARIABLE(ok);
  }

  // reset event and clear ec.
  // usually used before initiate winhttp api call.
  void step_reset() {
    HANDLE h = this->step_event.native_handle();
    assert(h != NULL);
    assert(h != INVALID_HANDLE_VALUE);
    bool ok = ResetEvent(h);
    assert(ok);
    DBG_UNREFERENCED_LOCAL_VARIABLE(ok);
    step_ec.clear();
    step_len = 0;
  }

  net::windows::basic_object_handle<executor_type> step_event;
  boost::system::error_code step_ec;
  std::size_t step_len;

  executor_type get_executor() { return ex_; }

private:
  executor_type ex_;
  state state_;
};

template <typename Executor>
void __stdcall BasicAsioAsyncCallback(HINTERNET hInternet, DWORD_PTR dwContext,
                                      DWORD dwInternetStatus,
                                      LPVOID lpvStatusInformation,
                                      DWORD dwStatusInformationLength) {
  UNREFERENCED_PARAMETER(hInternet);
  typedef asio_request_context<Executor>::state ctx_state_type;
  asio_request_context<Executor> *cpContext;
  cpContext = (asio_request_context<Executor> *)dwContext;

  boost::system::error_code ec;

  if (cpContext == NULL) {
    // this should not happen, but we are being defensive here
    return;
  }

  // Create a string that reflects the status flag.
  switch (dwInternetStatus) {
  case WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE: {
    // Data is available to be retrieved with WinHttpReadData.The
    // lpvStatusInformation parameter points to a DWORD that contains the number
    // of bytes of data available. The dwStatusInformationLength parameter
    // itself is 4 (the size of a DWORD).

    BOOST_ASSERT(dwStatusInformationLength == sizeof(DWORD));
    DWORD data_len = *((LPDWORD)lpvStatusInformation);

    spdlog::debug("DATA_AVAILABLE {}", data_len);

    // call back needs to finish request if len is 0
    BOOST_ASSERT(cpContext->get_state() == ctx_state_type::data_available);
    cpContext->step_complete(ec, data_len);
  } break;
  case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE:
    // The response header has been received and is available with
    // WinHttpQueryHeaders. The lpvStatusInformation parameter is NULL.
    spdlog::debug("HEADERS_AVAILABLE {}", dwStatusInformationLength);
    // Begin downloading the resource.
    BOOST_ASSERT(cpContext->get_state() == ctx_state_type::headers_available);
    cpContext->step_complete(ec);
    break;
  case WINHTTP_CALLBACK_STATUS_READ_COMPLETE:
    // Data was successfully read from the server. The lpvStatusInformation
    // parameter contains a pointer to the buffer specified in the call to
    // WinHttpReadData. The dwStatusInformationLength parameter contains the
    // number of bytes read. When used by WinHttpWebSocketReceive, the
    // lpvStatusInformation parameter contains a pointer to a
    // WINHTTP_WEB_SOCKET_STATUS structure, 	and the
    // dwStatusInformationLength
    // parameter indicates the size of lpvStatusInformation.
    spdlog::debug("READ_COMPLETE Number of bytes read {}",
                  dwStatusInformationLength);
    // Copy the data and delete the buffers.
    BOOST_ASSERT(cpContext->get_state() == ctx_state_type::read_complete);
    cpContext->step_complete(ec, dwStatusInformationLength);
    break;
  case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:
    spdlog::debug("SENDREQUEST_COMPLETE {}", dwStatusInformationLength);
    // Prepare the request handle to receive a response.
    BOOST_ASSERT(cpContext->get_state() == ctx_state_type::send_request);
    cpContext->step_complete(ec);
    break;
  case WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE: {
    BOOST_ASSERT(cpContext->get_state() == ctx_state_type::write_complete);
    BOOST_ASSERT(dwStatusInformationLength == sizeof(DWORD));
    DWORD data_len = *((LPDWORD)lpvStatusInformation);
    spdlog::debug("WRITE_COMPLETE len: {}", data_len);
    cpContext->step_complete(ec, data_len);
  } break;
  case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR: {
    WINHTTP_ASYNC_RESULT *pAR = (WINHTTP_ASYNC_RESULT *)lpvStatusInformation;
    std::wstring err;
    winnet::winhttp::error::get_api_error_str(pAR, err);
    spdlog::debug(L"winhttp callback error: {}", err);
    ec.assign(pAR->dwError, boost::asio::error::get_system_category());
    BOOST_ASSERT(ec.failed()); // ec must be failed.
    switch (cpContext->get_state()) {
    case ctx_state_type::data_available:
      BOOST_ASSERT(pAR->dwResult == API_QUERY_DATA_AVAILABLE);
      cpContext->step_complete(ec, 0);
      break;
    case ctx_state_type::headers_available:
      BOOST_ASSERT(pAR->dwResult == API_RECEIVE_RESPONSE);
      cpContext->step_complete(ec);
      break;
    case ctx_state_type::read_complete:
      BOOST_ASSERT(pAR->dwResult == API_READ_DATA);
      cpContext->step_complete(ec, 0);
      break;
    case ctx_state_type::send_request:
      BOOST_ASSERT(pAR->dwResult == API_SEND_REQUEST);
      cpContext->step_complete(ec);
      break;
    case ctx_state_type::write_complete:
      BOOST_ASSERT(pAR->dwResult == API_WRITE_DATA);
      cpContext->step_complete(ec, 0);
      break;
    default:
      // API_GET_PROXY_FOR_URL is not used.
      spdlog::debug("winhttp callback error unknown state num: {}",
                    pAR->dwResult);
      BOOST_ASSERT_MSG(false, "Unknown error winhttp callback error state" +
                                  pAR->dwResult);
    }
  } break;
  default:
    spdlog::debug("Unknown/unhandled callback - status {}", dwInternetStatus);
    BOOST_ASSERT_MSG(false, "dwInternetStatus unknown.");
    break;
  }
}

} // namespace details

// wrapper for requeset handle to perform async operations
template <typename Executor = net::any_io_executor>
class basic_winhttp_request_asio_handle
    : public basic_winhttp_request_handle<Executor> {
public:
  /// The type of the executor associated with the object.
  typedef Executor executor_type;

  typedef basic_winhttp_request_handle<executor_type> parent_type;

  typedef details::asio_request_context<executor_type>::state ctx_state_type;

  explicit basic_winhttp_request_asio_handle(const executor_type &ex)
      : basic_winhttp_request_handle<executor_type>(ex), ctx_(ex) {
    // set call back
  }

  // open request using manged asio callback and ctx
  // TODO: dwFlags WINHTTP_FLAG_AUTOMATIC_CHUNKING can be used for streaming.
  void managed_open(HINTERNET hConnect, LPCWSTR pwszVerb,
                    LPCWSTR pwszObjectName, LPCWSTR pwszVersion,
                    LPCWSTR pwszReferrer, LPCWSTR *ppwszAcceptTypes,
                    DWORD dwFlags, boost::system::error_code &ec) {
    this->open(hConnect, pwszVerb, pwszObjectName, pwszVersion, pwszReferrer,
               ppwszAcceptTypes, dwFlags, ec);
    if (ec) {
      return;
    }
    // using managed callback
    this->set_status_callback(
        (WINHTTP_STATUS_CALLBACK)details::BasicAsioAsyncCallback<executor_type>,
        WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS, ec);
    if (ec) {
      return;
    }
  }

  // easy managed open using managed asio callback and ctx
  // open is always sychronous.
  void managed_open(HINTERNET hConnect, LPCWSTR method, LPCWSTR path,
                    boost::system::error_code &ec) {
    managed_open(hConnect, method, path,
                 NULL, // http 1.1
                 WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                 WINHTTP_FLAG_SECURE, ec);
  }

  // all async handler are of type void(ec, size_t)

  // one needs to call sync open before send.
  // async has all the same param with sync version
  // but has handler token.
  // winttp callback case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE
  // handler signature is void(boost::system::error_code)
  // handler should call async_recieve_response
  template <typename Handler>
  auto async_send(LPCWSTR lpszHeaders, DWORD dwHeadersLength, LPVOID lpOptional,
                  DWORD dwOptionalLength, DWORD dwTotalLength,
                  Handler &&token) {
    boost::system::error_code ec;
    ctx_.set_state(ctx_state_type::send_request);
    // this is optional.
    ctx_.step_reset();
    parent_type::send(lpszHeaders, dwHeadersLength, lpOptional,
                      dwOptionalLength, dwTotalLength, (DWORD_PTR)&ctx_, ec);
    if (ec) {
      ctx_.set_state(ctx_state_type::error);
      // set the event so that the async op will complete immediately.
      ctx_.step_complete(ec);
    }

    // defer to winhttp to invoke callback
    // callback/token is passed to a windows event/object handle in ctx
    return boost::asio::async_compose<Handler, void(boost::system::error_code)>(
        details::async_op<executor_type>(&ctx_.step_event, &ctx_.step_ec),
        token, ctx_.step_event);
  }

  // callback case: WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE
  // handler signature is void(boost::system::error_code)
  // handler should call async_query_data_available
  template <typename Handler> auto async_recieve_response(Handler &&token) {
    boost::system::error_code ec;
    ctx_.set_state(ctx_state_type::headers_available);

    ctx_.step_reset();
    parent_type::receive_response(ec);
    if (ec) {
      ctx_.set_state(ctx_state_type::error);
      ctx_.step_complete(ec);
    }

    return boost::asio::async_compose<Handler, void(boost::system::error_code)>(
        details::async_op<executor_type>(&ctx_.step_event, &ctx_.step_ec),
        token, ctx_.step_event);
  }

  // can be invoke in async_recieve_response or ...
  // callback case : WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE
  // handler signature is void(boost::system::error_code, std::size_t)
  // handler should call async_read_data
  // if callback len is 0, this means body/request has ended.
  template <typename Handler> auto async_query_data_available(Handler &&token) {
    boost::system::error_code ec;
    ctx_.set_state(ctx_state_type::data_available);

    ctx_.step_reset();
    parent_type::query_data_available(NULL, ec);
    if (ec) {
      ctx_.set_state(ctx_state_type::error);
      ctx_.step_complete(ec, 0);
    }
    return boost::asio::async_compose<Handler, void(boost::system::error_code,
                                                    std::size_t)>(
        details::async_len_op<executor_type>(&ctx_.step_event, &ctx_.step_ec,
                                             &ctx_.step_len),
        token, ctx_.step_event);
  }

  // callback case: WINHTTP_CALLBACK_STATUS_READ_COMPLETE
  // handler signature is void(boost::system::error_code, std::size_t)
  // handler should call async_query_data_available
  template <typename Handler>
  auto async_read_data(_Out_ LPVOID lpBuffer, _In_ DWORD dwNumberOfBytesToRead,
                       Handler &&token) {
    boost::system::error_code ec;
    ctx_.set_state(ctx_state_type::read_complete);
    ctx_.step_reset();
    parent_type::read_data(lpBuffer, dwNumberOfBytesToRead,
                           NULL, // lpdwNumberOfBytesRead
                           ec);
    if (ec) {
      ctx_.set_state(ctx_state_type::error);
      ctx_.step_complete(ec, 0);
    }
    return boost::asio::async_compose<Handler, void(boost::system::error_code,
                                                    std::size_t)>(
        details::async_len_op<executor_type>(&ctx_.step_event, &ctx_.step_ec,
                                             &ctx_.step_len),
        token, ctx_.step_event);
  }

  // callback case: WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE
  // handler signature is void(boost::system::error_code, std::size_t)
  // user can call async_read_data again in token.
  template <typename Handler>
  auto async_write_data(_In_ LPCVOID lpBuffer,
                        _In_ DWORD dwNumberOfBytesToWrite, Handler &&token) {

    boost::system::error_code ec;
    ctx_.set_state(ctx_state_type::write_complete);
    ctx_.step_reset();
    parent_type::write_data(lpBuffer, dwNumberOfBytesToWrite,
                            NULL, // lpdwNumberOfBytesWritten
                            ec);
    if (ec) {
      ctx_.set_state(ctx_state_type::error);
      ctx_.step_complete(ec, 0);
    }
    return boost::asio::async_compose<Handler, void(boost::system::error_code,
                                                    std::size_t)>(
        details::async_len_op<executor_type>(&ctx_.step_event, &ctx_.step_ec,
                                             &ctx_.step_len),
        token, ctx_.step_event);
  }

private:
  details::asio_request_context<executor_type> ctx_;
};

namespace details {

// compose operation that reads all http body into buff
template <typename Executor, typename DynamicBuffer>
class async_read_body_op : boost::asio::coroutine {
public:
  typedef Executor executor_type;
  async_read_body_op(basic_winhttp_request_asio_handle<executor_type> &h,
                     DynamicBuffer &buff)
      : h_request_(h), buff_(buff), state_(state::idle) {}

  template <typename Self>
  void operator()(Self &self, boost::system::error_code ec = {},
                  std::size_t len = 0) {
    if (ec) {
      self.complete(ec, len);
      return;
    }

    switch (state_) {
    case state::idle:
      state_ = state::query_data;
      h_request_.async_query_data_available(std::move(self));
      break;
    case state::query_data: {
      // If query len = 0 -> no more data.
      // If do not need trailers, request done and success.
      // If need trailers, we need to call read data and it will return 0 bytes
      // read and populate trailers.
      // here we assume we need http trailers.
      state_ = state::read_data;
      auto buff = this->buff_.prepare(len + 1); // always prepare 1 more byte.
      h_request_.async_read_data((LPVOID)buff.data(), static_cast<DWORD>(len),
                                 std::move(self));
    } break;
    case state::read_data:
      if (len != 0) {
        this->buff_.commit(len);
        // Check for more data.
        state_ = state::query_data;
        h_request_.async_query_data_available(std::move(self));
      } else {
        spdlog::debug("state::read_data complete");
        state_ = state::done;
        self.complete(ec, 0); // TODO total len;
      }
      break;
    default:
      BOOST_ASSERT_MSG(false, "unknown state");
      break;
    }
  }

private:
  basic_winhttp_request_asio_handle<executor_type> &h_request_;
  DynamicBuffer &buff_;
  enum class state { idle, query_data, read_data, done } state_;
};
} // namespace details

// async read all body into buffer
// use this in async_recieve_response handler
// handler signature void(ec, size_t)
template <typename Executor, typename DynamicBuffer,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::system::error_code,
                                               std::size_t))
              Token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(Executor)>
auto async_read_body(basic_winhttp_request_asio_handle<Executor> &h,
                     DynamicBuffer &buffer, Token &&token) {
  return boost::asio::async_compose<Token, void(boost::system::error_code,
                                                std::size_t)>(
      details::async_read_body_op<Executor, DynamicBuffer>(h, buffer), token,
      h.get_executor());
}

} // namespace winhttp
} // namespace winasio
} // namespace boost