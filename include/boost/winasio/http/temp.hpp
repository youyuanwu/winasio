#ifndef UNICODE
#define UNICODE
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#include <http.h>

#include <boost/winasio/http/http.hpp>

#include <functional>
#include <iostream>
#include <stdio.h>
#include <string>
#include <vector>

namespace net = boost::asio;
namespace winnet = boost::winasio;

template <typename Executor = net::any_io_executor>
class http_connection
    : public std::enable_shared_from_this<http_connection<Executor>> {
public:
  typedef Executor executor_type;

  http_connection(winnet::http::basic_http_queue<executor_type> &queue_handle)
      : http_connection(queue_handle,
                        [](const winnet::http::simple_request &request,
                           winnet::http::simple_response &response) {
                          // default handler
                          response.set_status_code(503);
                          response.set_reason("Not Implemented");
                        }) {}

  http_connection(winnet::http::basic_http_queue<executor_type> &queue_handle,
                  std::function<void(const winnet::http::simple_request &,
                                     winnet::http::simple_response &)>
                      handler)
      : queue_handle_(queue_handle), request_(), response_(),
        handler_(handler) {}

  void start() {
    receive_request();
    // check_deadline();
  }

private:
  // request block
  winnet::http::simple_request request_;
  // response block
  winnet::http::simple_response response_;

  winnet::http::basic_http_queue<executor_type> &queue_handle_;

  std::function<void(const winnet::http::simple_request &,
                     winnet::http::simple_response &)>
      handler_;

  void receive_request() {
    BOOST_LOG_TRIVIAL(debug) << "http_connection receive_request";
    auto self = this->shared_from_this();
    // this is the vector<char>
    auto &dynamicbuff = request_.get_request_dynamic_buffer();

    winnet::http::async_receive(
        queue_handle_, dynamicbuff,
        [self](boost::system::error_code ec, std::size_t) {
          if (ec) {
            BOOST_LOG_TRIVIAL(debug)
                << "async_recieve_request failed: " << ec.message();
          } else {
            self->on_receive_request();
            // start another connection
            std::make_shared<http_connection>(self->queue_handle_,
                                              self->handler_)
                ->start();
          }
        });
  }

  void on_receive_request() {
    auto self = this->shared_from_this();
    auto &buff = request_.get_body_dynamic_buffer();
    winnet::http::async_receive_body(
        queue_handle_, request_.get_request_id(), buff,
        [self](boost::system::error_code ec, std::size_t len) {
          if (ec) {
            BOOST_LOG_TRIVIAL(debug)
                << "async_recieve_body failed: " << ec.message();
          } else {
            self->on_recieve_body();
          }
        });
  }

  void on_recieve_body() {
    // all request data is ready, invoke use handler
    this->handler_(this->request_, this->response_);

    auto self = this->shared_from_this();
    queue_handle_.async_send_response(
        response_.get_response(), request_.get_request_id(),
        HTTP_SEND_RESPONSE_FLAG_DISCONNECT, // single resp flag
        [self](boost::system::error_code ec, std::size_t) {
          if (ec) {
            BOOST_LOG_TRIVIAL(debug)
                << "async_send_response failed: " << ec.message();
          }
        });
  }
};
