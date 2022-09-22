#ifndef BOOST_WINASIO_BASIC_HTTP_QUEUE_HPP
#define BOOST_WINASIO_BASIC_HTTP_QUEUE_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
#pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <http.h>

#include <boost/asio/detail/config.hpp>
#include <boost/asio/windows/basic_overlapped_handle.hpp>
#include <boost/asio/windows/overlapped_ptr.hpp>

#include <boost/log/trivial.hpp>

#include <iostream>

namespace boost {
namespace winasio {
namespace http {
namespace net = boost::asio;
namespace winnet = net::windows;

template <typename Executor = net::any_io_executor>
class basic_http_queue : public winnet::basic_overlapped_handle<Executor> {
public:
  /// The type of the executor associated with the object.
  typedef Executor executor_type;

  /// Rebinds the handle type to another executor.
  template <typename Executor1> struct rebind_executor {
    /// The handle type when rebound to the specified executor.
    typedef basic_http_queue<Executor1> other;
  };

  /// The native representation of a handle.
#if defined(GENERATING_DOCUMENTATION)
  typedef implementation_defined native_handle_type;
#else
  typedef boost::asio::detail::win_iocp_handle_service::native_handle_type
      native_handle_type;
#endif

  explicit basic_http_queue(const executor_type &ex)
      : winnet::basic_overlapped_handle<Executor>(ex) {}

  template <typename ExecutionContext>
  explicit basic_http_queue(
      ExecutionContext &context,
      typename net::constraint<
          net::is_convertible<ExecutionContext &,
                              net::execution_context &>::value,
          net::defaulted_constraint>::type = net::defaulted_constraint())
      : winnet::basic_overlapped_handle<Executor>(context) {}

  basic_http_queue(const executor_type &ex, const native_handle_type &handle)
      : winnet::basic_overlapped_handle<Executor>(ex, handle) {}

  template <typename ExecutionContext>
  basic_http_queue(
      ExecutionContext &context, const native_handle_type &handle,
      typename net::constraint<net::is_convertible<
          ExecutionContext &, net::execution_context &>::value>::type = 0)
      : winnet::basic_overlapped_handle<Executor>(context, handle) {}

  basic_http_queue(basic_http_queue &&other)
      : winnet::basic_overlapped_handle<Executor>(std::move(other)) {}

  basic_http_queue &operator=(basic_http_queue &&other) {
    winnet::basic_overlapped_handle<Executor>::operator=(std::move(other));
    return *this;
  }

  void add_url(std::wstring const &url, boost::system::error_code &ec) {
    DWORD retCode = HttpAddUrl(this->native_handle(), // Req Queue
                               url.c_str(),           // Fully qualified URL
                               NULL                   // Reserved
    );
    // std::wcout << L"added url " << url << std::endl;
    ec = boost::system::error_code(retCode,
                                   boost::asio::error::get_system_category());
  }

  void remove_url(std::wstring const &url, boost::system::error_code &ec) {
    DWORD retCode = HttpRemoveUrl(this->native_handle(), // Req Queue
                                  url.c_str()            // Fully qualified URL
    );
    // std::wcout << L"removed url " << url << std::endl;
    ec = boost::system::error_code(retCode,
                                   boost::asio::error::get_system_category());
  }

  template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::system::error_code,
                                                 std::size_t)) ReadHandler
                BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(ReadHandler,
                                     void(boost::system::error_code,
                                          std::size_t))
  async_recieve_request(
      _In_ HTTP_REQUEST_ID RequestId, _In_ ULONG Flags,
      _Out_ PHTTP_REQUEST RequestBuffer, _In_ ULONG RequestBufferLength,
      BOOST_ASIO_MOVE_ARG(ReadHandler)
          handler BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)) {
    BOOST_LOG_TRIVIAL(debug)
        << L"async_recieve_request buff len " << RequestBufferLength;

    boost::asio::windows::overlapped_ptr optr(this->get_executor(),
                                              std::move(handler));
    ULONG result =
        HttpReceiveHttpRequest(this->native_handle(), // Req Queue
                               RequestId,             // Req ID
                               Flags,                 // Flags.
                               RequestBuffer,         // HTTP request buffer
                               RequestBufferLength,   // req buffer length
                               NULL, // bytes received. must be null in async
                               optr.get() // LPOVERLAPPED
        );

    if (result == NO_ERROR) {
      BOOST_LOG_TRIVIAL(debug) << L"async_recieve_request is synchronous";
      // TODO: investigate if this should be a release() or complete().
      // This needs future testing. If iocp is corrupted, this might be the
      // reason.
      // optr.complete(ec, 0);
      optr.release();
      return;
    } else if (result == ERROR_IO_PENDING) {
      optr.release();
      return;
    } else {
      // error cases
      // note that ERROR_HANDLE_EOF should be handled by caller
      boost::system::error_code ec;
      ec = boost::system::error_code(result,
                                     boost::asio::error::get_system_category());
      // insufficient buff means that the buffer is smaller than
      // sizeof(HTTP_REQUEST). this lib does not handle this case, and user must
      // pass in buff at least of this size.
      BOOST_ASSERT(result != ERROR_INSUFFICIENT_BUFFER);
      optr.complete(ec, 0);
    }
  }

  template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::system::error_code,
                                                 std::size_t)) ReadHandler
                BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(ReadHandler,
                                     void(boost::system::error_code,
                                          std::size_t))
  async_recieve_body(
      _In_ HTTP_REQUEST_ID RequestId, _In_ ULONG Flags,
      _Out_ PVOID EntityBuffer, _In_ ULONG EntityBufferLength,
      BOOST_ASIO_MOVE_ARG(ReadHandler)
          handler BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)) {
    BOOST_LOG_TRIVIAL(debug)
        << L"async_recieve_body buff len " << EntityBufferLength;
    boost::asio::windows::overlapped_ptr optr(this->get_executor(),
                                              std::move(handler));
    DWORD result =
        HttpReceiveRequestEntityBody(this->native_handle(), RequestId, Flags,
                                     EntityBuffer, EntityBufferLength,
                                     NULL, //    BytesReturned,
                                     optr.get());
    boost::system::error_code ec;
    if (result == ERROR_IO_PENDING) {
      optr.release();
    } else if (result == NO_ERROR) {
      // TODO: caller needs to call this recieve body again.
      // until eof is reached.
      optr.release();
    } else {
      // ERROR_HANDLE_EOF needs to be handled by caller
      ec = boost::system::error_code(result,
                                     boost::asio::error::get_system_category());
      optr.complete(ec, 0);
    }
  }

  template <BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::system::error_code,
                                                 std::size_t)) WriteHandler
                BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(WriteHandler,
                                     void(boost::system::error_code,
                                          std::size_t))
  async_send_response(
      PHTTP_RESPONSE resp, HTTP_REQUEST_ID requestId, ULONG flags,
      BOOST_ASIO_MOVE_ARG(WriteHandler)
          handler BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)) {
    BOOST_LOG_TRIVIAL(debug) << "async_send_response";
    boost::asio::windows::overlapped_ptr optr(this->get_executor(), handler);
    DWORD result = HttpSendHttpResponse(
        this->native_handle(), // ReqQueueHandle
        requestId,             // Request ID
        flags,                 // Flags todo tweak this
        resp,                  // HTTP response
        NULL,                  // pReserved1
        NULL,                  // bytes sent  (OPTIONAL) must be null in async
        NULL,                  // pReserved2  (must be NULL)
        0,                     // Reserved3   (must be 0)
        optr.get(),            // LPOVERLAPPED(OPTIONAL)
        NULL                   // pReserved4  (must be NULL)
    );
    boost::system::error_code ec;
    if (result == ERROR_IO_PENDING) {
      optr.release();
    } else if (result == NO_ERROR) {
      // Note: this is different from namedpipe connect
      // if there is no error, we still get notification from iocp
      // do not complete here, else iocp is corrupted.
      optr.release();
    } else {
      // error
      ec = boost::system::error_code(result,
                                     boost::asio::error::get_system_category());
      optr.complete(ec, 0);
    }
  }

  void send_response(PHTTP_RESPONSE resp, HTTP_REQUEST_ID requestId,
                     ULONG flags, boost::system::error_code &ec) {
    BOOST_LOG_TRIVIAL(debug) << "send_response";
    ULONG bytesSent;
    DWORD result = HttpSendHttpResponse(
        this->native_handle(), // ReqQueueHandle
        requestId,             // Request ID
        flags,                 // Flags todo tweak this
        resp,                  // HTTP response
        NULL,                  // pReserved1
        &bytesSent,            // bytes sent  (OPTIONAL) must be null in async
        NULL,                  // pReserved2  (must be NULL)
        0,                     // Reserved3   (must be 0)
        NULL,                  // LPOVERLAPPED(OPTIONAL)
        NULL                   // pReserved4  (must be NULL)
    );
    ec = boost::system::error_code(result,
                                   boost::asio::error::get_system_category());
  }

  void shutdown(boost::system::error_code &ec) {
    DWORD result = HttpShutdownRequestQueue(this->native_handle());
    ec = boost::system::error_code(result,
                                   boost::asio::error::get_system_category());
  }
};
} // namespace http
} // namespace winasio
} // namespace boost

#endif // BOOST_WINASIO_BASIC_HTTP_QUEUE_HPP