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

See [examples](https://github.com/youyuanwu/winasio/tree/main/examples/named_pipe) for usage.

## Windows Http Api (http.sys)
[Windows Http Api](https://docs.microsoft.com/en-us/windows/win32/http/http-server-api-overview)

Utilize asio overlapped io intergration with executors.
Provide asio style wrapper for async operation with http.sys.

## Winhttp
[Winhttp](https://docs.microsoft.com/en-us/windows/win32/winhttp/winhttp-start-page)

Provide asio style wrapper for Winhttp running in async mode.

## Internals
See [Internals.md](docs/Internals.md) for implmentation details.

# Dependency
[asio/boost-asio](https://github.com/boostorg/asio)
Install as part of boost: [boost windows install](https://www.boost.org/doc/libs/1_80_0/more/getting_started/windows.html)

# Licensing
Boost Software License - Version 1.0