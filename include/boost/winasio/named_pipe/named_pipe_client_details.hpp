//
// Copyright (c) 2022 Youyuan Wu
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_NAMED_PIPE_CLIENT_DETAILS_HPP
#define ASIO_NAMED_PIPE_CLIENT_DETAILS_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
#pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

namespace boost {
namespace winasio {

namespace details {

// client connect to the namedpipe,
// return the ok handle. Caller is responsible for freeing the handle.
inline void client_connect(boost::system::error_code &ec, HANDLE &pipe_ret,
                           std::string const &endpoint, int timeout_ms) {

  HANDLE hPipe;
  BOOL fSuccess = FALSE;
  DWORD dwMode;
  DWORD last_error = {};
  // connect to pipe
  while (1) {
    hPipe = CreateFile(endpoint.c_str(), // pipe name
                       GENERIC_READ |    // read and write access
                           GENERIC_WRITE,
                       0,                    // no sharing
                       NULL,                 // default security attributes
                       OPEN_EXISTING,        // opens existing pipe
                       FILE_FLAG_OVERLAPPED, // default attributes
                       NULL);                // no template file

    // Break if the pipe handle is valid.

    if (hPipe != INVALID_HANDLE_VALUE)
      break;

    // Exit if an error other than ERROR_PIPE_BUSY occurs.

    last_error = ::GetLastError();
    if (last_error != ERROR_PIPE_BUSY) {
      // printf("Could not open pipe. GLE=%d\n", GetLastError());
      ec = boost::system::error_code(last_error,
                                     boost::asio::error::get_system_category());
      return;
    }

    // All pipe instances are busy, so wait for 20 seconds.

    if (!WaitNamedPipe(endpoint.c_str(), timeout_ms)) {
      printf("Could not open pipe: %d second wait timed out.",
             timeout_ms / 1000);
      // return -1;
      last_error = ::GetLastError();
      ec = boost::system::error_code(last_error,
                                     boost::asio::error::get_system_category());
      return;
    }
  }

  // The pipe connected; change to message-read mode.
  dwMode = PIPE_READMODE_MESSAGE;
  fSuccess = SetNamedPipeHandleState(hPipe,   // pipe handle
                                     &dwMode, // new pipe mode
                                     NULL,    // don't set maximum bytes
                                     NULL);   // don't set maximum time
  if (!fSuccess) {
    // printf("SetNamedPipeHandleState failed. GLE=%d\n", GetLastError());
    last_error = ::GetLastError();
    ec =
        boost::system::error_code(last_error, boost::system::system_category());
    return;
  }
  pipe_ret = hPipe;
}
} // namespace details
} // namespace winasio
} // namespace boost
#endif // ASIO_NAMED_PIPE_CLIENT_DETAILS_HPP