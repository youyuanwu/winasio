
#ifndef ASIO_NAMED_PIPE_ACCEPTOR_HPP
#define ASIO_NAMED_PIPE_ACCEPTOR_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include "winasio/named_pipe.hpp"
#include "winasio/named_pipe_server_details.hpp"
#include "boost/asio/compose.hpp"
#include "boost/asio/windows/basic_object_handle.hpp"

namespace boost{
namespace winasio{

template <typename Executor>
class named_pipe_acceptor{
public:
    /// The type of the executor associated with the object.
    typedef Executor executor_type;

    /// Rebinds the acceptor type to another executor.
    template <typename Executor1>
    struct rebind_executor
    {
        /// The socket type when rebound to the specified executor.
        typedef named_pipe_acceptor<Executor1> other;
    };

    typedef std::string endpoint_type;

    template<typename ExecutionContext>
    explicit named_pipe_acceptor(ExecutionContext& context, const endpoint_type endpoint,
    typename boost::asio::constraint<
        boost::asio::is_convertible<ExecutionContext&, boost::asio::execution_context&>::value
      >::type = 0):
        endpoint_(endpoint), executor_(context.get_executor()), pipe_(context), o_(context)
    {
    }

    explicit named_pipe_acceptor(executor_type& ex, const endpoint_type endpoint):
        endpoint_(endpoint), executor_(ex), pipe_(ex), o_(ex)
    {
    }

    // accept client connection into pipe. pipe needs to be moved to user session
    // by the user so the the 
    // next client connection can be accepted in to pipe again.
    // handler signature: void(error_code)
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code))
        AcceptToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
        BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(AcceptToken,
        void (asio::error_code))
    async_accept(
        server_named_pipe<executor_type> & pipe,
        BOOST_ASIO_MOVE_ARG(AcceptToken) token
        BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)){

        return boost::asio::async_compose<AcceptToken, void (boost::system::error_code)>(
            details::async_accept_op<executor_type>(&pipe, this->endpoint_), token, pipe
        );
    }

    // TODO: fix move and rebind executor
    // handler signature: void(error_code, pipe)
    template <
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
            server_named_pipe<executor_type>)) MoveAcceptToken
                BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(MoveAcceptToken,
        void (boost::system::error_code, 
            server_named_pipe<executor_type>))
    async_accept(
        BOOST_ASIO_MOVE_ARG(MoveAcceptToken) token
            BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
    {
        // std::cout << "async_accept func" << std::endl;

        return boost::asio::async_compose<MoveAcceptToken, void (boost::system::error_code, server_named_pipe<executor_type>)>(
            details::async_move_accept_op<executor_type>(&this->pipe_, this->endpoint_), token, this->pipe_
        );
    }

    const endpoint_type endpoint_;
private:
    const executor_type & executor_;
    // for move accept, this holds the pipe.
    server_named_pipe<executor_type> pipe_;

    // shared event
    boost::asio::windows::basic_object_handle<executor_type> o_;
};

} // namespace asio
} // namespace boost

#endif // ASIO_NAMED_PIPE_ACCEPTOR_HPP