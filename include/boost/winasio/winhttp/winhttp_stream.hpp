#pragma once

#include "boost/winasio/winhttp/winhttp_asio.hpp"

namespace boost {
namespace winasio {
namespace winhttp {

namespace net = boost::asio; // from <boost/asio.hpp>
namespace winnet = boost::winasio;

namespace details {

// compose operation that reads some data.
// This only read once, but not all data
template <typename Executor> class async_read_some_op : boost::asio::coroutine {
public:
  typedef Executor executor_type;
  // in net::async_read, the passed in MutableBufferSequence is on stack, and
  // can be destroyed in compose second run, so we use raw data and len of the
  // buffer for this compose operation.
  async_read_some_op(basic_winhttp_request_asio_handle<executor_type> *h,
                     void *data, std::size_t size)
      : h_(h), data_(data), size_(size), state_(state::idle) {}

  template <typename Self>
  void operator()(Self &self, boost::system::error_code ec = {},
                  std::size_t len = 0) {
    if (ec) {
      self.complete(ec, len);
      return;
    }

    switch (state_) {
    case state::idle:
      state_ = state::query_data;
      h_->async_query_data_available(std::move(self));
      break;
    case state::query_data: {
      state_ = state::read_data;
      //  read bytes into data buff
      std::size_t read_len = std::min(len, size_);
      h_->async_read_data((LPVOID)data_, static_cast<DWORD>(read_len),
                          std::move(self));
    } break;
    case state::read_data:
      state_ = state::done;
      // seems like net::async_read will stop if read len is 0 automatically.
      self.complete(ec, len);
      break;
    default:
      BOOST_ASSERT_MSG(false, "unknown state");
      break;
    }
  }

private:
  basic_winhttp_request_asio_handle<executor_type> *h_;
  // data and len of the mutable buffer
  void *data_;
  std::size_t size_;
  enum class state { idle, query_data, read_data, done } state_;
};

template <typename Executor>
class async_write_some_op : boost::asio::coroutine {
public:
  typedef Executor executor_type;
  async_write_some_op(basic_winhttp_request_asio_handle<executor_type> *h,
                      const void *data, std::size_t size)
      : h_(h), data_(data), size_(size), state_(state::idle) {}

  template <typename Self>
  void operator()(Self &self, boost::system::error_code ec = {},
                  std::size_t len = 0) {
    if (ec) {
      self.complete(ec, len);
      return;
    }

    switch (state_) {
    case state::idle:
      state_ = state::write_data;
      h_->async_write_data((LPCVOID)data_, static_cast<DWORD>(size_),
                           std::move(self));
      break;
    case state::write_data:
      state_ = state::done;
      self.complete(ec, len);
      break;
    default:
      BOOST_ASSERT_MSG(false, "unknown state");
      break;
    }
  }

private:
  basic_winhttp_request_asio_handle<executor_type> *h_;
  // data and len of const buffer
  // we can use const buffer sequence reference here, but for consistency of the
  // read op, we use raw types.
  const void *data_;
  std::size_t size_;
  enum class state { idle, write_data, done } state_;
};

} // namespace details

// wrapper for asio operations to impl asio stream
template <typename Executor = net::any_io_executor>
class basic_winhttp_request_stream_handle
    : public basic_winhttp_request_asio_handle<Executor> {
public:
  /// The type of the executor associated with the object.
  typedef Executor executor_type;

  explicit basic_winhttp_request_stream_handle(const executor_type &ex)
      : basic_winhttp_request_asio_handle<executor_type>(ex) {}

  template <typename MutableBufferSequence,
            BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::system::error_code,
                                                 std::size_t)) ReadToken
                BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(ReadToken, void(boost::system::error_code,
                                                     std::size_t))
  async_read_some(
      const MutableBufferSequence &buffers,
      BOOST_ASIO_MOVE_ARG(ReadToken)
          token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)) {

    return boost::asio::async_compose<ReadToken, void(boost::system::error_code,
                                                      std::size_t)>(
        details::async_read_some_op<Executor>(this, buffers.data(),
                                              buffers.size()),
        token, this->get_executor());
  }

  template <typename ConstBufferSequence,
            BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::system::error_code,
                                                 std::size_t)) WriteToken
                BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(WriteToken, void(boost::system::error_code,
                                                      std::size_t))
  async_write_some(
      const ConstBufferSequence &buffers,
      BOOST_ASIO_MOVE_ARG(WriteToken)
          token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type)) {
    return boost::asio::async_compose<
        WriteToken, void(boost::system::error_code, std::size_t)>(
        details::async_write_some_op<Executor>(this, buffers.data(),
                                               buffers.size()),
        token, this->get_executor());
  }
};

} // namespace winhttp
} // namespace winasio
} // namespace boost