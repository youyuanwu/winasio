#ifndef BOOST_WINASIO_HTTP_ASIO_HPP
#define BOOST_WINASIO_HTTP_ASIO_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
#pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

// compose operations for http
#include <boost/winasio/http/basic_http_queue.hpp>

namespace boost {
namespace winasio {
namespace http {

namespace net = boost::asio;

// convert asio mutable buffer to request
template <typename MutableBufferSequence>
PHTTP_REQUEST phttp_request(const MutableBufferSequence &buffer) {
  return (PHTTP_REQUEST)buffer.data();
}

template <typename MutableBufferSequence>
ULONG phttp_request_size(const MutableBufferSequence &buffer) {
  return static_cast<ULONG>(buffer.size());
}

namespace details {

template <typename Executor, typename DynamicBuffer>
class async_receive_op : boost::asio::coroutine {
public:
  typedef Executor executor_type;
  async_receive_op(basic_http_queue<executor_type> &h, DynamicBuffer &buff)
      : h_(h), buff_(buff), state_(state::idle) {}

  template <typename Self>
  void operator()(Self &self, boost::system::error_code ec = {},
                  std::size_t len = 0) {
    switch (state_) {
    case state::idle: {
      if (ec) {
        self.complete(ec, len);
      } else {
        state_ = state::recieving;
        this->recieve(self, sizeof(HTTP_REQUEST) + 1024, true); // initial size
      }
    } break;
    case state::recieving: {
      if (ec) {
        if (ec.value() == ERROR_HANDLE_EOF) {
          // request finished success
          BOOST_LOG_TRIVIAL(debug)
              << L"async_receive_op ERROR_HANDLE_EOF with len " << len;
          self.complete(boost::system::error_code{}, len);
        } else if (ec.value() == ERROR_MORE_DATA) {
          BOOST_LOG_TRIVIAL(debug)
              << L"async_receive_op ERROR_MORE_DATA with len " << len;
          // resize mutbuff and try again. still in recieving state.
          BOOST_ASSERT_MSG(len > buff_.size(), "len should increase");
          this->recieve(self, len, false);
        } else {
          // genuine error
          self.complete(ec, len);
        }
      } else {
        // success
        BOOST_LOG_TRIVIAL(debug)
            << L"async_receive_op NO_ERROR with len " << len;
        // final commit len might be smaller than requested len. Maybe some
        // space was used for intermediate calculation
        buff_.commit(len);
        self.complete(ec, len);
      }
    } break;
    }
  }

private:
  basic_http_queue<executor_type> &h_;
  DynamicBuffer &buff_;
  enum class state { idle, recieving } state_;

  // helper to recieve request with buff size len
  template <typename Self>
  void recieve(Self &self, std::size_t len, bool init) {
    auto mutbuff = buff_.prepare(len);
    PHTTP_REQUEST pRequest = phttp_request(mutbuff);
    size_t size = phttp_request_size(mutbuff);
    if (init) {
      HTTP_SET_NULL_ID(&pRequest->RequestId);
    }
    h_.async_recieve_request(pRequest->RequestId,
                             0, // flags
                             pRequest, static_cast<ULONG>(size),
                             std::move(self));
  }
};

template <typename Executor, typename DynamicBuffer>
class async_receive_body_op : boost::asio::coroutine {
public:
  typedef Executor executor_type;
  async_receive_body_op(basic_http_queue<executor_type> &h, HTTP_REQUEST_ID id,
                        DynamicBuffer &buff)
      : h_(h), id_(id), buff_(buff), state_(state::idle) {}

  template <typename Self>
  void operator()(Self &self, boost::system::error_code ec = {},
                  std::size_t len = 0) {
    switch (state_) {
    case state::idle: {
      if (ec) {
        self.complete(ec, len);
      } else {
        state_ = state::recieving;
        // initial prepare 10 bytes
        this->recieve_body(self, 10);
      }
      break;
    case state::recieving:
      if (ec) {
        if (ec.value() == ERROR_HANDLE_EOF) {
          BOOST_LOG_TRIVIAL(debug) << L"async_receive_body_op ERROR_HANDLE_EOF";
          buff_.commit(len);
          self.complete(boost::system::error_code{}, len);
        } else {
          // genuine error
          self.complete(ec, len);
        }
      } else {
        // no error, need to call recieve again. still in recieving state.
        buff_.commit(len);
        this->recieve_body(self, 10);
      }
      break;
    }
    }
  }

private:
  basic_http_queue<executor_type> &h_;
  HTTP_REQUEST_ID id_;
  DynamicBuffer &buff_;
  enum class state { idle, recieving } state_;

  // helper to initiate receive.
  template <typename Self> void recieve_body(Self &self, std::size_t len) {
    auto mutbuff = buff_.prepare(len);
    h_.async_recieve_body(id_,
                          0, // flag
                          (PVOID)mutbuff.data(),
                          static_cast<ULONG>(mutbuff.size()), std::move(self));
  }
};

} // namespace details

// async recieve request, headers only
template <typename Executor, typename DynamicBuffer,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::system::error_code,
                                               std::size_t))
              Token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(Executor)>
auto async_receive(basic_http_queue<Executor> &h, DynamicBuffer &buffer,
                   Token &&token) {

  return boost::asio::async_compose<Token, void(boost::system::error_code,
                                                std::size_t)>(
      details::async_receive_op<Executor, DynamicBuffer>(h, buffer), token, h);
}

// async recieve body
template <typename Executor, typename DynamicBuffer,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::system::error_code,
                                               std::size_t))
              Token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(Executor)>
auto async_receive_body(basic_http_queue<Executor> &h, HTTP_REQUEST_ID id,
                        DynamicBuffer &buffer, Token &&token) {

  return boost::asio::async_compose<Token, void(boost::system::error_code,
                                                std::size_t)>(
      details::async_receive_body_op<Executor, DynamicBuffer>(h, id, buffer),
      token, h);
}

} // namespace http
} // namespace winasio
} // namespace boost

#endif // BOOST_WINASIO_HTTP_ASIO
