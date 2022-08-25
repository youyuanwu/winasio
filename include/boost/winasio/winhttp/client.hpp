#pragma once

#include "boost/winasio/winhttp/winhttp.hpp"
#include "boost/winasio/winhttp/winhttp_asio.hpp"

namespace boost {
namespace winasio {
namespace http {

namespace net = boost::asio; // from <boost/asio.hpp>
namespace winnet = boost::winasio;

// response to be filled, and exposed to user
template <typename Executor = net::any_io_executor> class response {
public:
  typedef Executor executor_type;
  response(
      winnet::http::basic_winhttp_request_asio_handle<executor_type> &h_request,
      boost::asio::dynamic_vector_buffer<BYTE, std::allocator<BYTE>> &buff)
      : h_request_(h_request), buff_(buff) {}

  // get view of body
  net::const_buffers_1 get_body() { return buff_.data(); }

  winnet::http::basic_winhttp_request_asio_handle<executor_type> &get_handle() {
    return h_request_;
  }

  std::string get_body_str() {
    // TODO: invalidate buffer?
    auto resp_body = get_body();
    auto view = resp_body.data();
    auto size = resp_body.size();
    return std::string((BYTE *)view, (BYTE *)view + size);
  }

private:
  winnet::http::basic_winhttp_request_asio_handle<executor_type> &h_request_;
  boost::asio::dynamic_vector_buffer<BYTE, std::allocator<BYTE>> &buff_;
};

template <typename Executor = net::any_io_executor>
class request2 : public std::enable_shared_from_this<request2<Executor>> {
public:
  typedef Executor executor_type;

  request2(const executor_type &ex) : h_request_(ex), body_(), buff_(body_) {}

  // synchronously open requst.
  void open(
      _In_ winnet::http::basic_winhttp_connect_handle<executor_type> &h_connect,
      const std::wstring &method, const std::optional<std::wstring> &path,
      winnet::http::header::accept_types &accept,
      _Out_ boost::system::error_code &ec) {
    LPCWSTR ppath = NULL;
    if (path) {
      ppath = path->c_str();
    }
    h_request_.managed_open(
        h_connect.native_handle(), method.c_str(), ppath, NULL, // http 1.1
        WINHTTP_NO_REFERER, (LPCWSTR *)accept.get(), WINHTTP_FLAG_SECURE, ec);
  }

  // wraper for simple api
  void async_exec(
      const winnet::http::header::headers &header,
      const std::optional<std::string> &body,
      std::function<void(boost::system::error_code, response<executor_type> &)>
          token) {

    LPVOID pbody = WINHTTP_NO_REQUEST_DATA;
    DWORD body_len = 0;
    if (body) {
      pbody = (LPVOID)body->data();
      body_len = static_cast<DWORD>(body->size());
    }

    this->async_exec(header.get(), header.size(), pbody, body_len, body_len,
                     token);
  }

  // start send request until end
  void async_exec(
      LPCWSTR lpszHeaders, DWORD dwHeadersLength, LPVOID lpOptional,
      DWORD dwOptionalLength, DWORD dwTotalLength,
      std::function<void(boost::system::error_code, response<executor_type> &)>
          token) {
    auto self = this->shared_from_this();
    user_token_ = token;
    h_request_.async_send(
        lpszHeaders, dwHeadersLength, lpOptional, dwOptionalLength,
        dwTotalLength, [self](boost::system::error_code ec) {
          BOOST_LOG_TRIVIAL(debug) << "async_send handler" << ec;
          if (ec) {
            self->cleanup(ec);
            return;
          }
          self->on_send_complete();
        });
  }

  void on_send_complete() {
    auto self = this->shared_from_this();
    h_request_.async_recieve_response([self](boost::system::error_code ec) {
      BOOST_LOG_TRIVIAL(debug) << "async_recieve_response handler" << ec;
      if (ec) {
        self->cleanup(ec);
        return;
      }
      self->query_data();
    });
  }

  void query_data() {
    auto self = this->shared_from_this();
    h_request_.async_query_data_available(
        [self](boost::system::error_code ec, std::size_t len) {
          BOOST_LOG_TRIVIAL(debug)
              << "async_query_data_available handler" << ec << "len " << len;
          if (ec || len == 0) {
            // all data read. request finished. or errored out
            self->cleanup(ec);
            return;
          }

          self->on_query_data_available_complete(len);
        });
  }

  void on_query_data_available_complete(std::size_t len) {
    auto self = this->shared_from_this();
    auto buff = this->buff_.prepare(len + 1);
    h_request_.async_read_data(
        (LPVOID)buff.data(), static_cast<DWORD>(len),
        [self](boost::system::error_code ec, std::size_t len) {
          BOOST_LOG_TRIVIAL(debug)
              << "async_read_data handler " << ec << "len " << len;
          if (ec) {
            self->cleanup(ec);
            return;
          }
          self->on_read_data_complete(len);
        });
  }

  void on_read_data_complete(std::size_t len) {
    if (len == 0) {
      // TODO: ??
      return;
    }
    this->buff_.commit(len);
    // Check for more data.
    this->query_data();
  }

  // ec is argument
  void cleanup(boost::system::error_code ec) {
    BOOST_LOG_TRIVIAL(debug) << "my request cleanup " << ec;

    // cleanup callback
    boost::system::error_code ec_internal;
    this->h_request_.set_status_callback(NULL, ec_internal);
    BOOST_ASSERT(!ec_internal.failed());

    // invoke user callback if exists
    if (user_token_) {
      net::post(h_request_.get_executor(),
                std::bind(user_token_, ec, response(h_request_, buff_)));
    }

    // notify asio request completes.
    this->h_request_.complete(ec_internal);
    BOOST_ASSERT(!ec_internal.failed());
  }

  winnet::http::basic_winhttp_request_asio_handle<executor_type> &
  get_request_handle() {
    return h_request_;
  }

private:
  winnet::http::basic_winhttp_request_asio_handle<executor_type> h_request_;
  std::vector<BYTE> body_;
  boost::asio::dynamic_vector_buffer<BYTE, std::allocator<BYTE>> buff_;

  std::function<void(boost::system::error_code, response<executor_type> &)>
      user_token_;
};

} // namespace http
} // namespace winasio
} // namespace boost