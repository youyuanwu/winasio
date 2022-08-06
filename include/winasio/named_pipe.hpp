// Named pipe for server and client

#ifndef ASIO_NAMED_PIPE_HPP
#define ASIO_NAMED_PIPE_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <boost/asio/detail/config.hpp>
#include <boost/asio/detail/type_traits.hpp>
#include "boost/asio/any_io_executor.hpp"
#include "boost/asio/windows/stream_handle.hpp"
#include "boost/asio/windows/overlapped_ptr.hpp"

#include "winasio/named_pipe_client_details.hpp"

#include <list>
#include <memory>
#include <map>

#include <iostream> //debug

namespace boost{
namespace winasio {

// maybe just a alias to stream handle

// template <typename Executor = any_io_executor>
// class named_pipe_protocol;

template <typename Executor = boost::asio::any_io_executor>
class named_pipe_acceptor;

// pipe for server
template <typename Executor = boost::asio::any_io_executor>
class server_named_pipe : public boost::asio::windows::basic_stream_handle<Executor> {
public:
    typedef Executor executor_type;

    /// Rebinds the socket type to another executor.
    template <typename Executor1>
    struct rebind_executor
    {
        /// The socket type when rebound to the specified executor.
        typedef server_named_pipe<Executor1> other;
    };

    server_named_pipe(executor_type& ex) : boost::asio::windows::basic_stream_handle<executor_type>(ex){}

    template<typename ExecutionContext>
    server_named_pipe(ExecutionContext& context, typename boost::asio::constraint<
        boost::asio::is_convertible<ExecutionContext&, boost::asio::execution_context&>::value
      >::type = 0) : boost::asio::windows::basic_stream_handle<executor_type>(context.get_executor()){}

    server_named_pipe(server_named_pipe&& other)
    : boost::asio::windows::basic_stream_handle<executor_type>(std::move(other))
    {
        //std::cout<< "pipe moved" << std::endl;
    }

    ~server_named_pipe()
    {
        using pipe = boost::asio::windows::basic_stream_handle<executor_type>;
        if(pipe::is_open())
        {
            // TODO: check error;
            // Flush is needed since the server might close handle before client read.
            FlushFileBuffers(pipe::native_handle());
            pipe::cancel();
            if(!DisconnectNamedPipe(pipe::native_handle())){
                boost::system::error_code ec = boost::system::error_code(static_cast<int>(GetLastError()), boost::system::system_category());
                std::cout << "disconnecting np failed" << ec.message() <<std::endl;
            }
        }
    }
};


template <typename Executor = boost::asio::any_io_executor>
class client_named_pipe : public boost::asio::windows::basic_stream_handle<Executor>{
public:
    typedef Executor executor_type;
    typedef std::string endpoint_type;

    client_named_pipe(executor_type& ex) : boost::asio::windows::basic_stream_handle<executor_type>(ex){}

    template<typename ExecutionContext>
    client_named_pipe(ExecutionContext& context, typename boost::asio::constraint<
        boost::asio::is_convertible<ExecutionContext&, boost::asio::execution_context&>::value
      >::type = 0) : boost::asio::windows::basic_stream_handle<executor_type>(context.get_executor()){}

    BOOST_ASIO_SYNC_OP_VOID connect(const endpoint_type& endpoint,
        boost::system::error_code& ec, int timeout_ms = 20000){
        
        if(boost::asio::windows::basic_stream_handle<executor_type>::is_open()){
           boost::asio::windows::basic_stream_handle<executor_type>::close();
        }

        HANDLE hPipe = NULL;
        details::client_connect(ec, hPipe, endpoint, timeout_ms);

        if(ec)
        {
            BOOST_ASIO_SYNC_OP_VOID_RETURN(ec); 
        }
        // assign the pipe to super class
        boost::asio::windows::basic_stream_handle<executor_type>::assign(hPipe);
        BOOST_ASIO_SYNC_OP_VOID_RETURN(ec);    
    }

    void connect(const endpoint_type &endpoint) {
        boost::system::error_code ec;
        this->connect(endpoint,ec);
        boost::asio::detail::throw_error(ec, "connect");
    }
};

} // namespace asio
} // namespace boost

#endif // ASIO_NAMED_PIPE_HPP