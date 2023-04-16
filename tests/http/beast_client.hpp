//
// Copyright (c) 2022 Youyuan Wu
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#pragma once

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>

namespace beast = boost::beast; // from <boost/beast.hpp>
namespace http = beast::http;   // from <boost/beast/http.hpp>
namespace net = boost::asio;    // from <boost/asio.hpp>
using tcp = net::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

#include <map>
#include <string>

class test_request {
public:
  inline void add_header(std::string key, std::string val) {
    headers_[key] = val;
  }

  inline void set_body(std::string body) { body_ = body; }

  // private:
  http::verb verb_ = http::verb::get;
  std::map<std::string, std::string> headers_;
  std::string body_;
};

class test_response {
public:
  int status_code;
  http::status status;
  std::string body;

private:
};

inline boost::system::error_code make_test_request(test_request const &request,
                                                   test_response &response) {
  // The io_context is required for all I/O
  net::io_context ioc;

  // These objects perform our I/O
  tcp::resolver resolver(ioc);
  beast::tcp_stream stream(ioc);

  // Look up the domain name
  auto const results = resolver.resolve("localhost", "12356");

  // Make the connection on the IP address we get from a lookup
  stream.connect(results);

  // Set up an HTTP request message
  http::request<http::string_body> req{request.verb_, "/winhttpapitest", 11};
  req.set(http::field::host, "localhost");
  req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
  req.body() = request.body_;
  req.prepare_payload();

  // Set custom header
  for (auto &kv : request.headers_) {
    req.set(kv.first, kv.second);
  }

  // Send the HTTP request to the remote host
  http::write(stream, req);

  // This buffer is used for reading and must be persisted
  beast::flat_buffer buffer;

  // Declare a container to hold the response
  http::response<http::dynamic_body> res;

  // Receive the HTTP response
  http::read(stream, buffer, res);

  // Write the message to standard out
  // std::cout << res << std::endl;

  // Gracefully close the socket
  beast::error_code ec;
  stream.socket().shutdown(tcp::socket::shutdown_both, ec);

  // not_connected happens sometimes
  // so don't bother reporting it.
  //
  if (ec && ec != beast::errc::not_connected) {
    return ec;
  }

  response.status = res.result();
  response.body = boost::beast::buffers_to_string(res.body().data());

  return beast::error_code{};
}