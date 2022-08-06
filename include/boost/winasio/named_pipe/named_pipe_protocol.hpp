
#ifndef ASIO_NAMED_PIPE_PROTOCOL_HPP
#define ASIO_NAMED_PIPE_PROTOCOL_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "boost/winasio/named_pipe/named_pipe.hpp"
#include "boost/winasio/named_pipe/named_pipe_acceptor.hpp"

namespace boost{
namespace winasio{

template <typename Executor = any_io_executor>
class named_pipe_protocol{
public:
    typedef named_pipe_acceptor<Executor> acceptor;

    typedef server_named_pipe<Executor> server_pipe;

    typedef client_named_pipe<Executor> client_pipe;

    typedef std::string endpoint;
};

} // namespace asio
}// namespace boost

#endif // ASIO_NAMED_PIPE_PROTOCOL_HPP