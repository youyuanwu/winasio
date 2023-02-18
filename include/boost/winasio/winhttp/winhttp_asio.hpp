#pragma once

// aims to have asio style apis
// send_header(handle, token)
#include "boost/winasio/winhttp/winhttp.hpp"
#include <functional>

namespace boost {
namespace winasio {
namespace winhttp {

namespace net = boost::asio; // from <boost/asio.hpp>
namespace winnet = boost::winasio;

// Dev note: when those optr are reset with a token(in user thread), the
// executor has an entry for this token and will wait for complete to be called
// on optr(from winhttp thread). So the contract between user init/send thread
// and winhttp callback thread are synchronized by this optr, and internally
// should be backed by a iocp. Original implementation added a event object on
// the h_request, and the event is created when request is sent, and executor
// must wait for the complete signal for this event which user needs to invoke
// when request is done. This is no longer needed and has been removed.

template <typename Executor> class asio_request_context {
public:
  typedef Executor executor_type;
  enum class state {
    idle,
    send_request,
    headers_available,
    data_available,
    read_complete,
    error
  };

  // callbacks
  asio_request_context(const executor_type &ex)
      : ex_(ex), on_send_request_complete(), on_headers_available(),
        on_data_available(), on_read_complete(), state_(state::idle) {}

  void set_state(state s) noexcept { state_ = s; }

  state get_state() const noexcept { return state_; }

  // need to invoke recieve response inside
  net::windows::overlapped_ptr on_send_request_complete;

  // need to invoke query data inside
  net::windows::overlapped_ptr on_headers_available;

  // need to invoke read data inside
  // if len is 0, request ends.
  net::windows::overlapped_ptr on_data_available;

  // need to invoke query data inside
  net::windows::overlapped_ptr on_read_complete;

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

#ifdef WINASIO_LOG
    BOOST_LOG_TRIVIAL(debug) << L"DATA_AVAILABLE " << data_len;
#endif

    // call back needs to finish request if len is 0
    BOOST_ASSERT(cpContext->get_state() == ctx_state_type::data_available);
    cpContext->on_data_available.complete(ec, data_len);
  } break;
  case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE:
    // The response header has been received and is available with
    // WinHttpQueryHeaders. The lpvStatusInformation parameter is NULL.
#ifdef WINASIO_LOG
    BOOST_LOG_TRIVIAL(debug)
        << L"HEADERS_AVAILABLE " << dwStatusInformationLength;
#endif
    // Begin downloading the resource.
    BOOST_ASSERT(cpContext->get_state() == ctx_state_type::headers_available);
    cpContext->on_headers_available.complete(ec, 0);
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
#ifdef WINASIO_LOG
    BOOST_LOG_TRIVIAL(debug)
        << "READ_COMPLETE Number of bytes read" << dwStatusInformationLength;
#endif
    // Copy the data and delete the buffers.
    BOOST_ASSERT(cpContext->get_state() == ctx_state_type::read_complete);
    cpContext->on_read_complete.complete(ec, dwStatusInformationLength);
    break;
  case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:
#ifdef WINASIO_LOG
    BOOST_LOG_TRIVIAL(debug)
        << L"SENDREQUEST_COMPLETE " << dwStatusInformationLength;
#endif
    // Prepare the request handle to receive a response.
    BOOST_ASSERT(cpContext->get_state() == ctx_state_type::send_request);
    cpContext->on_send_request_complete.complete(ec, 0);
    break;
  case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR: {
    WINHTTP_ASYNC_RESULT *pAR = (WINHTTP_ASYNC_RESULT *)lpvStatusInformation;
    std::wstring err;
    winnet::winhttp::error::get_api_error_str(pAR, err);
#ifdef WINASIO_LOG
    BOOST_LOG_TRIVIAL(debug) << "winhttp callback error: " << err;
#endif
    ec.assign(pAR->dwError, boost::asio::error::get_system_category());
    BOOST_ASSERT(ec.failed()); // ec must be failed.
    switch (cpContext->get_state()) {
    case ctx_state_type::data_available:
      BOOST_ASSERT(pAR->dwResult == API_QUERY_DATA_AVAILABLE);
      cpContext->on_data_available.complete(ec, 0);
    case ctx_state_type::headers_available:
      BOOST_ASSERT(pAR->dwResult == API_RECEIVE_RESPONSE);
      cpContext->on_headers_available.complete(ec, 0);
    case ctx_state_type::read_complete:
      BOOST_ASSERT(pAR->dwResult == API_READ_DATA);
      cpContext->on_read_complete.complete(ec, 0);
    case ctx_state_type::send_request:
      BOOST_ASSERT(pAR->dwResult == API_SEND_REQUEST);
      cpContext->on_send_request_complete.complete(ec, 0);
    default:
      // TODO: WinHttpWriteData is not used yet.
      // API_GET_PROXY_FOR_URL is not used.
#ifdef WINASIO_LOG
      BOOST_LOG_TRIVIAL(debug)
          << "winhttp callback error unknown state num: " << pAR->dwResult;
#endif
      BOOST_ASSERT_MSG(false, "Unknown error winhttp callback error state" +
                                  pAR->dwResult);
    }
  } break;
  default:
#ifdef WINASIO_LOG
    BOOST_LOG_TRIVIAL(debug) << L"Unknown/unhandled callback - status "
                             << dwInternetStatus << L"given";
#endif
    BOOST_ASSERT_MSG(false, "dwInternetStatus unknown.");
    break;
  }
}

// wrapper for requeset handle to perform async operations
template <typename Executor = net::any_io_executor>
class basic_winhttp_request_asio_handle
    : public basic_winhttp_request_handle<Executor> {
public:
  /// The type of the executor associated with the object.
  typedef Executor executor_type;

  typedef basic_winhttp_request_handle<executor_type> parent_type;

  typedef asio_request_context<executor_type>::state ctx_state_type;

  explicit basic_winhttp_request_asio_handle(const executor_type &ex)
      : basic_winhttp_request_handle<executor_type>(ex), ctx_(ex) {
    // set call back
  }

  // open request using manged asio callback and ctx
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
        (WINHTTP_STATUS_CALLBACK)BasicAsioAsyncCallback<executor_type>,
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
  // size_t param is not used
  template <typename Handler>
  void async_send(LPCWSTR lpszHeaders, DWORD dwHeadersLength, LPVOID lpOptional,
                  DWORD dwOptionalLength, DWORD dwTotalLength,
                  Handler &&token) {
    boost::system::error_code ec;
    // set callback in ctx.
    // defer to winhttp to invoke callback
    ctx_.on_send_request_complete.reset(ctx_.get_executor(), std::move(token));
    ctx_.set_state(ctx_state_type::send_request);
    parent_type::send(lpszHeaders, dwHeadersLength, lpOptional,
                      dwOptionalLength, dwTotalLength, (DWORD_PTR)&ctx_, ec);
    if (ec) {
      ctx_.set_state(ctx_state_type::error);
      ctx_.on_send_request_complete.complete(ec, 0);
    }
  }

  // callback case: WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE
  // size_t param is not used
  template <typename Handler> void async_recieve_response(Handler &&token) {
    boost::system::error_code ec;
    ctx_.set_state(ctx_state_type::headers_available);
    ctx_.on_headers_available.reset(ctx_.get_executor(), std::move(token));
    parent_type::receive_response(ec);
    if (ec) {
      ctx_.set_state(ctx_state_type::error);
      ctx_.on_headers_available.complete(ec, 0);
    }
  }

  // can be invoke in async_recieve_response or ...
  // callback case : WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE
  template <typename Handler> void async_query_data_available(Handler &&token) {
    boost::system::error_code ec;
    ctx_.set_state(ctx_state_type::data_available);
    ctx_.on_data_available.reset(ctx_.get_executor(), std::move(token));
    parent_type::query_data_available(NULL, ec);
    if (ec) {
      ctx_.set_state(ctx_state_type::error);
      ctx_.on_data_available.complete(ec, 0);
    }
  }

  // callback case: WINHTTP_CALLBACK_STATUS_READ_COMPLETE
  template <typename Handler>
  void async_read_data(_Out_ LPVOID lpBuffer, _In_ DWORD dwNumberOfBytesToRead,
                       Handler &&token) {
    boost::system::error_code ec;
    ctx_.set_state(ctx_state_type::read_complete);
    ctx_.on_read_complete.reset(ctx_.get_executor(), std::move(token));
    parent_type::read_data(lpBuffer, dwNumberOfBytesToRead,
                           NULL, // lpdwNumberOfBytesRead
                           ec);
    if (ec) {
      ctx_.set_state(ctx_state_type::error);
      ctx_.on_read_complete.complete(ec, 0);
    }
  }

private:
  asio_request_context<executor_type> ctx_;
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
      // no more data.
      // If do not need trailers, request done and success.
      // If need trailers, we need to call read data and it will return 0 bytes
      // read and populate trailers.
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
#ifdef WINASIO_LOG
        BOOST_LOG_TRIVIAL(debug) << L"state::read_data complete";
#endif
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