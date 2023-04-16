//
// Copyright (c) 2022 Youyuan Wu
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef ASIO_NAMED_PIPE_PROTOCOL_HPP
#define ASIO_NAMED_PIPE_PROTOCOL_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
#pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "boost/winasio/named_pipe/named_pipe.hpp"
#include "boost/winasio/named_pipe/named_pipe_acceptor.hpp"

namespace boost {
namespace winasio {

template <typename Executor = any_io_executor> class named_pipe_protocol {
public:
  typedef named_pipe_acceptor<Executor> acceptor;

  typedef named_pipe<Executor> pipe;

  typedef std::string endpoint;
};

} // namespace winasio
} // namespace boost

#endif // ASIO_NAMED_PIPE_PROTOCOL_HPP