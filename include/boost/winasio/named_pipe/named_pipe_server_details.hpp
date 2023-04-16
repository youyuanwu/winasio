//
// Copyright (c) 2022 Youyuan Wu
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_NAMED_PIPE_DETAILS_HPP
#define ASIO_NAMED_PIPE_DETAILS_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
#pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "boost/asio/coroutine.hpp"

#include <iostream>

namespace boost {
namespace winasio {
namespace details {

// handler of signature void(error_code, pipe)
template <typename Executor>
class async_move_accept_op : boost::asio::coroutine {
public:
  typedef std::string endpoint_type;

  async_move_accept_op(named_pipe<Executor> *pipe, endpoint_type endpoint)
      : pipe_(pipe), endpoint_(endpoint) {}

  template <typename Self>
  void operator()(Self &self, boost::system::error_code ec = {}) {
    // std::cout << "async_move_accept_op" << std::endl;
    if (ec) {
      // std::cout << "async_move_accept_op has error" << std::endl;
      self.complete(ec, std::move(*pipe_));
      return;
    }
    // create named pipe
    pipe_->server_create(ec, endpoint_);
    if (ec) {
      self.complete(ec, std::move(*pipe_));
      return;
    }
    // connect to namedpipe
    pipe_->async_server_connect(
        [self = std::move(self), p = pipe_](boost::system::error_code ec,
                                            std::size_t) mutable {
          self.complete(ec, std::move(*p));
        });
  }

private:
  // pipe for movable case is the pipe holder in acceptor, which needs to moved
  // to handler function, so that to free acceptor pipe holder to handle the
  // next connection.
  named_pipe<Executor> *pipe_;
  endpoint_type const endpoint_;
};

// handler of signature void(error_code)
template <typename Executor> class async_accept_op : boost::asio::coroutine {
public:
  typedef std::string endpoint_type;

  async_accept_op(named_pipe<Executor> *pipe, endpoint_type endpoint)
      : pipe_(pipe), endpoint_(endpoint) {}

  template <typename Self>
  void operator()(Self &self, boost::system::error_code ec = {}) {
    // std::cout << "async_move_accept_op" << std::endl;
    if (ec) {
      // std::cout << "async_move_accept_op has error" << std::endl;
      self.complete(ec);
      return;
    }
    // create named pipe
    pipe_->server_create(ec, endpoint_);
    if (ec) {
      self.complete(ec);
      return;
    }
    // connect to namedpipe
    pipe_->async_server_connect(
        [self = std::move(self)](boost::system::error_code ec,
                                 std::size_t) mutable { self.complete(ec); });
  }

private:
  // pipe for movable case is the pipe holder in acceptor, which needs to moved
  // to handler function, so that to free acceptor pipe holder to handle the
  // next connection.
  named_pipe<Executor> *pipe_;
  endpoint_type const endpoint_;
};

} // namespace details
} // namespace winasio
} // namespace boost

#endif // ASIO_NAMED_PIPE_DETAILS_HPP