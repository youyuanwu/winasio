#pragma once

#include <boost/asio/detail/config.hpp>
#include <boost/asio/windows/basic_overlapped_handle.hpp>

#include <boost/winasio/http/convert.hpp>

#include <iostream>

namespace boost {
namespace winasio {
namespace http {
namespace net = boost::asio;
namespace winnet = net::windows;

template <typename Executor = net::any_io_executor>
class basic_http_handle : public winnet::basic_overlapped_handle<Executor> {
public:
  /// The type of the executor associated with the object.
  typedef Executor executor_type;

  /// Rebinds the handle type to another executor.
  template <typename Executor1> struct rebind_executor {
    /// The handle type when rebound to the specified executor.
    typedef basic_http_handle<Executor1> other;
  };

  /// The native representation of a handle.
#if defined(GENERATING_DOCUMENTATION)
  typedef implementation_defined native_handle_type;
#else
  typedef boost::asio::detail::win_iocp_handle_service::native_handle_type
      native_handle_type;
#endif

  explicit basic_http_handle(const executor_type &ex)
      : winnet::basic_overlapped_handle<Executor>(ex) {}

  template <typename ExecutionContext>
  explicit basic_http_handle(
      ExecutionContext &context,
      typename net::constraint<
          net::is_convertible<ExecutionContext &,
                              net::execution_context &>::value,
          net::defaulted_constraint>::type = net::defaulted_constraint())
      : winnet::basic_overlapped_handle<Executor>(context) {}

  basic_http_handle(const executor_type &ex, const native_handle_type &handle)
      : winnet::basic_overlapped_handle<Executor>(ex, handle) {}

  template <typename ExecutionContext>
  basic_http_handle(
      ExecutionContext &context, const native_handle_type &handle,
      typename net::constraint<net::is_convertible<
          ExecutionContext &, net::execution_context &>::value>::type = 0)
      : winnet::basic_overlapped_handle<Executor>(context, handle) {}

  basic_http_handle(basic_http_handle &&other)
      : winnet::basic_overlapped_handle<Executor>(std::move(other)) {}

  basic_http_handle &operator=(basic_http_handle &&other) {
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

  template <typename MutableBufferSequence,
            BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::system::error_code,
                                                 std::size_t)) ReadHandler
                BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(ReadHandler,
                                     void(boost::system::error_code,
                                          std::size_t))
  async_recieve_request(
      const MutableBufferSequence &buffers,
      BOOST_ASIO_MOVE_ARG(ReadHandler)
          handler BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)) {
    boost::asio::windows::overlapped_ptr optr(this->get_executor(), handler);
    PHTTP_REQUEST pRequest = phttp_request(buffers);
    ULONG result = HttpReceiveHttpRequest(
        this->native_handle(),              // Req Queue
        pRequest->RequestId,                // Req ID
        0,                                  // Flags. TODO: expose this
        pRequest,                           // HTTP request buffer
        static_cast<ULONG>(buffers.size()), // req buffer length
        NULL,      // bytes received. must be null in async
        optr.get() // LPOVERLAPPED
    );
    boost::system::error_code ec;
    // TODO: handle more data???
    if (result == ERROR_HANDLE_EOF) {
      optr.complete(ec, 0);
    } else if (result == NO_ERROR) {
      std::cout << "recieve io ok synchronous" << std::endl;
      // TODO: investigate if this should be a release() or complete().
      // This needs future testing. If iocp is corrupted, this might be the
      // reason.
      // optr.complete(ec, 0);
      optr.release();
    } else if (result == ERROR_IO_PENDING) {
      optr.release();
    } else {
      // error
      ec = boost::system::error_code(result,
                                     boost::asio::error::get_system_category());
      optr.complete(ec, 0);
    }
  }

  template <typename MutableBufferSequence,
            BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::system::error_code,
                                                 std::size_t)) ReadHandler
                BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(ReadHandler,
                                     void(boost::system::error_code,
                                          std::size_t))
  async_recieve_body(
      const MutableBufferSequence &buffers, HTTP_REQUEST_ID requestId,
      ULONG flags,
      BOOST_ASIO_MOVE_ARG(ReadHandler)
          handler BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)) {
    boost::asio::windows::overlapped_ptr optr(this->get_executor(), handler);
    DWORD result = HttpReceiveRequestEntityBody(
        this->native_handle(), requestId, flags, (PVOID)buffers.data(),
        static_cast<ULONG>(buffers.size()),
        NULL, //    BytesReturned,
        optr.get());
    boost::system::error_code ec;
    if (result == ERROR_IO_PENDING) {
      // std::cout << "resp io pending" << std::endl;
      optr.release();
    } else if (result == NO_ERROR) {
      // TODO: caller needs to call this recieve body again.
      // until eof is reached.
      optr.release();
    } else if (result == ERROR_HANDLE_EOF) {
      // no more data. success.
      optr.complete(ec, 0);
    } else {
      // std::cout << "resp io error" << std::endl;
      // error
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
      std::cout << "resp io pending" << std::endl;
      optr.release();
    } else if (result == NO_ERROR) {
      std::cout << "resp io ok synchronous" << std::endl;
      // Note: this is different from namedpipe connect
      // if there is no error, we still get notification from iocp
      // do not complete here, else iocp is corrupted.
      optr.release();
    } else {
      std::cout << "resp io error" << std::endl;
      // error
      ec = boost::system::error_code(result,
                                     boost::asio::error::get_system_category());
      optr.complete(ec, 0);
    }
  }

  void send_response(PHTTP_RESPONSE resp, HTTP_REQUEST_ID requestId,
                     ULONG flags, boost::system::error_code &ec) {
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
};
} // namespace http
} // namespace winasio
} // namespace boost