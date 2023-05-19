# winasio

Windows extension for boost-asio. Header only. Alpha, WIP.
![ci](https://github.com/youyuanwu/winasio/actions/workflows/build.yaml/badge.svg)
[![codecov](https://codecov.io/gh/youyuanwu/winasio/branch/main/graph/badge.svg?token=O5ZTZ03DAN)](https://codecov.io/gh/youyuanwu/winasio)

# Compenents
## NamedPipe ASIO Server and Client
`boost::winasio::named_pipe_acceptor` is a drop in replacement for `boost::asio::basic_socket_acceptor`.

`boost::winasio::named_pipe` is a drop in replacement for `boost::asio::basic_stream_socket`.

One build a named_pipe server and client with all asio socket functionalities.

Counter part in other languages:
 * Golang `github.com/Microsoft/go-winio` [DialPipe](https://pkg.go.dev/github.com/microsoft/go-winio?GOOS=windows#DialPipe)
 * Rust tokio [tokio::net::windows::named_pipe](https://docs.rs/tokio/latest/tokio/net/windows/named_pipe/index.html)

Coroutine Example Snippet:
```cpp
  winnet::named_pipe_protocol<net::any_io_executor>::endpoint ep(
      "\\\\.\\pipe\\mynamedpipe");
  winnet::named_pipe_protocol<net::any_io_executor>::acceptor acceptor(executor,
                                                                       ep);
  for (;;) {
    winnet::named_pipe_protocol<net::any_io_executor>::pipe socket =
        co_await acceptor.async_accept(use_awaitable);
    std::cout << "listener spawing" << std::endl;
    co_spawn(executor, echo(std::move(socket)), detached);
  }
```
See [examples](examples/named_pipe) for full code.

## Windows Http Api (http.sys)
[Windows Http Api](https://docs.microsoft.com/en-us/windows/win32/http/http-server-api-overview)

Utilize asio overlapped io intergration with executors.
Provide asio style wrapper for async operation with http.sys.

## Winhttp
[Winhttp](https://docs.microsoft.com/en-us/windows/win32/winhttp/winhttp-start-page)

Provide asio style wrapper for Winhttp running in async mode. All winhttp apis are converted to async version, compatible with both coroutine and callback styles.

Coroutine Example Snippet:
```cpp
  co_await h_request.async_send(NULL,                    // headers
                                0,                       // header len
                                WINHTTP_NO_REQUEST_DATA, // optional
                                0,                       // optional len
                                0,                       // total len
                                net::use_awaitable);

  std::vector<BYTE> body_buff;
  auto dybuff = net::dynamic_buffer(body_buff);

  co_await h_request.async_recieve_response(net::use_awaitable);
  std::size_t len = 0;
  do {
    len =
        co_await h_request.async_query_data_available(net::use_awaitable);
    auto buff = dybuff.prepare(len + 1);
    std::size_t read_len = co_await h_request.async_read_data(
        (LPVOID)buff.data(), static_cast<DWORD>(len), net::use_awaitable);
    BOOST_REQUIRE_EQUAL(read_len, len);
    dybuff.commit(len);
  } while (len > 0);
```
See [Example Test](tests/winhttp/winhttp_request_test.cpp) for full code.

## Internals
See [Internals.md](docs/Internals.md) for implmentation details.

# Dependency
[asio/boost-asio](https://github.com/boostorg/asio)
Install as part of boost: [boost windows install](https://www.boost.org/doc/libs/1_80_0/more/getting_started/windows.html)

# Licensing
Boost Software License - Version 1.0