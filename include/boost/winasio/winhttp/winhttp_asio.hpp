#pragma once

// aims to have asio style apis
// send_header(handle, token)
#include "boost/winasio/winhttp/winhttp.hpp"
#include <functional>

namespace boost {
namespace winasio {
namespace http {

namespace net = boost::asio; // from <boost/asio.hpp>
namespace winnet = boost::winasio;

template <typename Executor> class asio_request_context {
public:
  typedef Executor executor_type;
  // callbacks
  asio_request_context(const executor_type &ex)
      : ex_(ex), on_send_request_complete(nullptr),
        on_headers_available(nullptr), on_data_available(nullptr),
        on_read_complete(nullptr) {}

  // need to invoke recieve response inside
  std::function<void(boost::system::error_code)> on_send_request_complete;

  // need to invoke query data inside
  std::function<void(boost::system::error_code)> on_headers_available;

  // need to invoke read data inside
  // if len is 0, request ends.
  std::function<void(boost::system::error_code, std::size_t)> on_data_available;

  // need to invoke query data inside
  std::function<void(boost::system::error_code, std::size_t)> on_read_complete;

  executor_type get_executor() { return ex_; }

private:
  executor_type ex_;
};

template <typename Executor>
void __stdcall BasicAsioAsyncCallback(HINTERNET hInternet, DWORD_PTR dwContext,
                                      DWORD dwInternetStatus,
                                      LPVOID lpvStatusInformation,
                                      DWORD dwStatusInformationLength) {
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

    BOOST_LOG_TRIVIAL(debug) << L"DATA_AVAILABLE " << data_len;

    // call back needs to finish request if len is 0
    BOOST_ASSERT(cpContext->on_data_available != nullptr);
    net::post(cpContext->get_executor(),
              std::bind(cpContext->on_data_available, ec, data_len));
    cpContext->on_data_available = nullptr;
  } break;
  case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE:
    // The response header has been received and is available with
    // WinHttpQueryHeaders. The lpvStatusInformation parameter is NULL.
    BOOST_LOG_TRIVIAL(debug)
        << L"HEADERS_AVAILABLE " << dwStatusInformationLength;
    // Begin downloading the resource.
    BOOST_ASSERT(cpContext->on_headers_available != nullptr);
    net::post(cpContext->get_executor(),
              std::bind(cpContext->on_headers_available, ec));
    cpContext->on_headers_available = nullptr;
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

    BOOST_LOG_TRIVIAL(debug)
        << "READ_COMPLETE Number of bytes read" << dwStatusInformationLength;
    // Copy the data and delete the buffers.

    BOOST_ASSERT(cpContext->on_read_complete != nullptr);
    net::post(
        cpContext->get_executor(),
        std::bind(cpContext->on_read_complete, ec, dwStatusInformationLength));
    cpContext->on_read_complete = nullptr;
    break;
  case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:
    BOOST_LOG_TRIVIAL(debug)
        << L"SENDREQUEST_COMPLETE " << dwStatusInformationLength;

    // Prepare the request handle to receive a response.
    BOOST_ASSERT(cpContext->on_send_request_complete != nullptr);
    net::post(cpContext->get_executor(),
              std::bind(cpContext->on_send_request_complete, ec));
    cpContext->on_send_request_complete = nullptr;
    break;
  default:
    BOOST_LOG_TRIVIAL(debug) << L"Unknown/unhandled callback - status "
                             << dwInternetStatus << L"given";
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
        (WINHTTP_STATUS_CALLBACK)
            winnet::http::BasicAsioAsyncCallback<executor_type>,
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

  // one needs to call sync open before send.
  // async has all the same param with sync version
  // but has handler token.
  // winttp callback case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE
  void async_send(LPCWSTR lpszHeaders, DWORD dwHeadersLength, LPVOID lpOptional,
                  DWORD dwOptionalLength, DWORD dwTotalLength,
                  std::function<void(boost::system::error_code)> token) {
    boost::system::error_code ec;
    // set callback in ctx.
    // defer to winhttp to invoke callback
    ctx_.on_send_request_complete = token;
    parent_type::send(lpszHeaders, dwHeadersLength, lpOptional,
                      dwOptionalLength, dwTotalLength, (DWORD_PTR)&ctx_, ec);
    if (ec) {
      net::post(this->get_executor(), std::bind(token, ec));
      ctx_.on_send_request_complete = nullptr;
    }
  }

  // callback case: WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE
  void
  async_recieve_response(std::function<void(boost::system::error_code)> token) {
    boost::system::error_code ec;
    ctx_.on_headers_available = token;
    parent_type::receive_response(ec);
    if (ec) {
      net::post(this->get_executor(), std::bind(token, ec));
      ctx_.on_headers_available = nullptr;
    }
  }

  // can be invoke in async_recieve_response or ...
  // callback case : WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE
  void async_query_data_available(
      std::function<void(boost::system::error_code, std::size_t)> token) {
    boost::system::error_code ec;
    ctx_.on_data_available = token;
    parent_type::query_data_available(NULL, ec);
    if (ec) {
      net::post(this->get_executor(), std::bind(token, ec, 0));
      ctx_.on_data_available = nullptr;
    }
  }

  // callback case: WINHTTP_CALLBACK_STATUS_READ_COMPLETE
  void async_read_data(
      _Out_ LPVOID lpBuffer, _In_ DWORD dwNumberOfBytesToRead,
      std::function<void(boost::system::error_code, size_t)> token) {
    boost::system::error_code ec;
    ctx_.on_read_complete = token;
    parent_type::read_data(lpBuffer, dwNumberOfBytesToRead,
                           NULL, // lpdwNumberOfBytesRead
                           ec);
    if (ec) {
      net::post(this->get_executor(), std::bind(token, ec, 0));
      ctx_.on_read_complete = nullptr;
    }
  }

private:
  asio_request_context<executor_type> ctx_;
};

} // namespace http
} // namespace winasio
} // namespace boost