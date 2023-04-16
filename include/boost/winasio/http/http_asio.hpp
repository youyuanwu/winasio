//
// Copyright (c) 2022 Youyuan Wu
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_WINASIO_HTTP_ASIO_HPP
#define BOOST_WINASIO_HTTP_ASIO_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
#pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

// compose operations for http
#include <boost/winasio/http/basic_http_queue_handle.hpp>

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
  async_receive_op(basic_http_queue_handle<executor_type> &h,
                   DynamicBuffer &buff)
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
#ifdef WINASIO_LOG
          BOOST_LOG_TRIVIAL(debug)
              << L"async_receive_op ERROR_HANDLE_EOF with len " << len;
#endif
          self.complete(boost::system::error_code{}, len);
        } else if (ec.value() == ERROR_MORE_DATA) {
#ifdef WINASIO_LOG
          BOOST_LOG_TRIVIAL(debug)
              << L"async_receive_op ERROR_MORE_DATA with len " << len;
#endif
          // resize mutbuff and try again. still in recieving state.
          BOOST_ASSERT_MSG(len > buff_.size(), "len should increase");
          this->recieve(self, len, false);
        } else {
          // genuine error
          self.complete(ec, len);
        }
      } else {
        // success
#ifdef WINASIO_LOG
        BOOST_LOG_TRIVIAL(debug)
            << L"async_receive_op NO_ERROR with len " << len;
#endif
        // final commit len might be smaller than requested len. Maybe some
        // space was used for intermediate calculation
        buff_.commit(len);
        self.complete(ec, len);
      }
    } break;
    }
  }

private:
  basic_http_queue_handle<executor_type> &h_;
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
  async_receive_body_op(basic_http_queue_handle<executor_type> &h,
                        HTTP_REQUEST_ID id, DynamicBuffer &buff,
                        std::size_t body_size_hint)
      : h_(h), id_(id), buff_(buff), state_(state::idle),
        body_size_hint_(body_size_hint) {}

  template <typename Self>
  void operator()(Self &self, boost::system::error_code ec = {},
                  std::size_t len = 0) {
    switch (state_) {
    case state::idle: {
      if (ec) {
        self.complete(ec, len);
      } else {
        state_ = state::recieving;
        // start recieve with 10 bytes.
        // we anyway need to call recieve again to get handle eof.
        this->recieve_body(self, body_size_hint_);
      }
      break;
    case state::recieving:
      if (ec) {
        if (ec.value() == ERROR_HANDLE_EOF) {
#ifdef WINASIO_LOG
          BOOST_LOG_TRIVIAL(debug) << L"async_receive_body_op ERROR_HANDLE_EOF";
#endif
          buff_.commit(len);
          self.complete(boost::system::error_code{}, len);
        } else {
          // genuine error
          self.complete(ec, len);
        }
      } else {
        // no error, need to call recieve again. still in recieving state.
        buff_.commit(len);
#ifdef WINASIO_LOG
        BOOST_LOG_TRIVIAL(debug) << "recieved len " << len;
#endif
        if (len < body_size_hint_) {
          // We use HTTP_RECEIVE_REQUEST_ENTITY_BODY_FLAG_FILL_BUFFER mode.
          // if buffer is not filled. Means that request body is small and
          // already handled by previous buffer. The next call should be EOF, so
          // we use a tiny buffer to avoid allocation. dynamic buffer should not
          // be resized.
          this->recieve_body(self, 1);
        } else {
          this->recieve_body(self, body_size_hint_);
        }
      }
      break;
    }
    }
  }

private:
  basic_http_queue_handle<executor_type> &h_;
  HTTP_REQUEST_ID id_;
  DynamicBuffer &buff_;
  std::size_t body_size_hint_;
  enum class state { idle, recieving } state_;

  // helper to initiate receive.
  template <typename Self> void recieve_body(Self &self, std::size_t len) {
    auto mutbuff = buff_.prepare(len);
    h_.async_recieve_body(
        id_,
        HTTP_RECEIVE_REQUEST_ENTITY_BODY_FLAG_FILL_BUFFER, // flag
        (PVOID)mutbuff.data(), static_cast<ULONG>(mutbuff.size()),
        std::move(self));
  }
};

} // namespace details

// async recieve request, headers only
template <typename Executor, typename DynamicBuffer,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::system::error_code,
                                               std::size_t))
              Token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(Executor)>
auto async_receive(basic_http_queue_handle<Executor> &h, DynamicBuffer &buffer,
                   Token &&token) {

  return boost::asio::async_compose<Token, void(boost::system::error_code,
                                                std::size_t)>(
      details::async_receive_op<Executor, DynamicBuffer>(h, buffer), token, h);
}

// async recieve body
// body_size_hint is to instruct http api to read body size at a time.
// ideally this size hint should be just enough to read body in one call.
template <typename Executor, typename DynamicBuffer,
          BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::system::error_code,
                                               std::size_t))
              Token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(Executor)>
auto async_receive_body(basic_http_queue_handle<Executor> &h,
                        HTTP_REQUEST_ID id, DynamicBuffer &buffer,
                        Token &&token, std::size_t body_size_hint = 128) {

  return boost::asio::async_compose<Token, void(boost::system::error_code,
                                                std::size_t)>(
      details::async_receive_body_op<Executor, DynamicBuffer>(h, id, buffer,
                                                              body_size_hint),
      token, h);
}

} // namespace http
} // namespace winasio
} // namespace boost

#endif // BOOST_WINASIO_HTTP_ASIO
