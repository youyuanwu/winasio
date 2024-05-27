//
// Copyright (c) 2022 Youyuan Wu
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// Named pipe for server and client

#ifndef ASIO_NAMED_PIPE_HPP
#define ASIO_NAMED_PIPE_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
#pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "boost/asio/any_io_executor.hpp"
#include "boost/asio/windows/overlapped_ptr.hpp"
#include "boost/asio/windows/stream_handle.hpp"
#include <boost/asio/detail/config.hpp>
#include <boost/asio/detail/type_traits.hpp>

#include "boost/winasio/named_pipe/named_pipe_client_details.hpp"

#include <list>
#include <map>
#include <memory>

#include <iostream> //debug

namespace boost {
namespace winasio {

template <typename Executor = boost::asio::any_io_executor>
class named_pipe : public boost::asio::windows::basic_stream_handle<Executor> {
public:
  typedef Executor executor_type;
  typedef std::string endpoint_type;
  typedef boost::asio::windows::basic_stream_handle<executor_type> parent_type;

  named_pipe(const executor_type &ex)
      : boost::asio::windows::basic_stream_handle<executor_type>(ex) {}

  template <typename ExecutionContext>
  named_pipe(
      ExecutionContext &context,
      typename boost::asio::constraint<boost::asio::is_convertible<
          ExecutionContext &, boost::asio::execution_context &>::value>::type =
          0)
      : boost::asio::windows::basic_stream_handle<executor_type>(
            context.get_executor()) {}

  named_pipe(named_pipe<executor_type> &&other)
      : boost::asio::windows::basic_stream_handle<executor_type>(
            std::move(other)) {}

  void server_create(boost::system::error_code &ec,
                     endpoint_type const &endpoint) {
    const int bufsize = 512;
    HANDLE hPipe =
        CreateNamedPipe(endpoint.c_str(),           // pipe name
                        PIPE_ACCESS_DUPLEX |        // read/write access
                            FILE_FLAG_OVERLAPPED,   // overlapped mode
                        PIPE_TYPE_MESSAGE |         // message-type pipe
                            PIPE_READMODE_MESSAGE | // message-read mode
                            PIPE_WAIT,              // blocking mode
                        PIPE_UNLIMITED_INSTANCES,   // number of instances
                        bufsize * sizeof(TCHAR),    // output buffer size
                        bufsize * sizeof(TCHAR),    // input buffer size
                        0,                          // client time-out
                        NULL); // default security attributes

    if (hPipe == INVALID_HANDLE_VALUE) {
      DWORD last_error = ::GetLastError();
      ec = boost::system::error_code(last_error,
                                     boost::asio::error::get_system_category());
      return;
    }
    parent_type::assign(hPipe);
  }

  template <typename Token> auto async_server_connect(Token &&token) {
    return boost::asio::async_initiate<decltype(token),
                                       void(boost::system::error_code)>(
        [this](auto handler) {
          // init optr to pass through the user handler.
          boost::asio::windows::overlapped_ptr optr(
              this->get_executor(),
              [h = std::move(handler)](boost::system::error_code ec,
                                       std::size_t) mutable {
                std::move(h)(ec);
              });
          boost::system::error_code ec;
          bool fConnected = false;
          fConnected = ConnectNamedPipe(this->native_handle(), optr.get());
          // Overlapped ConnectNamedPipe should return zero.
          if (fConnected) {
            // printf("ConnectNamedPipe failed with %d.\n", GetLastError());
            ec = boost::system::error_code(::GetLastError(),
                                           boost::system::system_category());
            optr.complete(ec, 0);
            return;
          }

          switch (GetLastError()) {
          // The overlapped connection in progress.
          case ERROR_IO_PENDING:
            optr.release();
            break;
          // Client is already connected, so signal an event.
          case ERROR_PIPE_CONNECTED: {
            // In the win32 example here we need to reset the overlapp event
            // when pipe already is connected. But this case overlapped_ptr
            // cannot trigger this because iocp does not register this pipe
            // instance.
            optr.complete(ec, 0);
            break;
          }
          // If an error occurs during the connect operation...
          default: {
            // printf("Some named pipe op failed with %d.\n", GetLastError());
            ec = boost::system::error_code(
                ::GetLastError(), boost::asio::error::get_system_category());
            optr.complete(ec, 0);
          }
          }
        },
        token);
  }

  // used for client to connect
  BOOST_ASIO_SYNC_OP_VOID connect(const endpoint_type &endpoint,
                                  boost::system::error_code &ec,
                                  std::uint32_t timeout_ms = 20000) {

    if (boost::asio::windows::basic_stream_handle<executor_type>::is_open()) {
      boost::asio::windows::basic_stream_handle<executor_type>::close();
    }

    HANDLE hPipe = NULL;
    details::client_connect(ec, hPipe, endpoint, timeout_ms);

    if (ec) {
      BOOST_ASIO_SYNC_OP_VOID_RETURN(ec);
    }
    // assign the pipe to super class
    boost::asio::windows::basic_stream_handle<executor_type>::assign(hPipe);
    BOOST_ASIO_SYNC_OP_VOID_RETURN(ec);
  }

  void connect(const endpoint_type &endpoint) {
    boost::system::error_code ec;
    this->connect(endpoint, ec);
    boost::asio::detail::throw_error(ec, "connect");
  }

  // shutdown the namedpipe
  void shutdown(boost::system::error_code &ec) {
    using pipe = boost::asio::windows::basic_stream_handle<executor_type>;
    if (pipe::is_open()) {
      // Flush is needed since the server might close handle before client read.
      if (!FlushFileBuffers(pipe::native_handle())) {
        ec = boost::system::error_code(static_cast<int>(GetLastError()),
                                       boost::system::system_category());
        return;
      }
      if (!DisconnectNamedPipe(pipe::native_handle())) {
        ec = boost::system::error_code(static_cast<int>(GetLastError()),
                                       boost::system::system_category());
      }
    }
  }
};

} // namespace winasio
} // namespace boost

#endif // ASIO_NAMED_PIPE_HPP