#pragma once

#include "boost/winasio/http/basic_http_handle.hpp"
#include "boost/winasio/http/convert.hpp"
#include "boost/winasio/http/http_asio.hpp"
//#include "boost/winasio/http/basic_http_request.hpp"
//#include "boost/winasio/http/basic_http_response.hpp"

#include <functional>
#include <map>

#pragma push_macro("DELETE")
#undef DELETE

namespace boost {
namespace winasio {
namespace http {

namespace net = boost::asio;

template <typename Executor = net::any_io_executor>
class basic_http_controller {

  using request_handler =
      std::function<void(const simple_request &, simple_response &)>;
  using request_response =
      std::pair<http::simple_request, http::simple_response>;

public:
  basic_http_controller(basic_http_handle<Executor> &queue,
                        const std::wstring &url_base)
      : queue_(queue), base_url_(format_url_base(url_base)) {
    boost::system::error_code ec;
    queue_.add_url(url_base, ec);
  }

  template <typename Handler>
  void get(const std::wstring &url_part, Handler &&h) {
    register_url_part<HTTP_VERB::HttpVerbGET>(url_part,
                                              std::forward<Handler>(h));
  }

  template <typename Handler>
  void post(const std::wstring &url_part, Handler &&h) {

    register_url_part<HTTP_VERB::HttpVerbPOST>(url_part,
                                               std::forward<Handler>(h));
  }

  template <typename Handler>
  void put(const std::wstring &url_part, Handler &&h) {

    register_url_part<HTTP_VERB::HttpVerbPUT>(url_part,
                                              std::forward<Handler>(h));
  }

  template <typename Handler>
  void del(const std::wstring &url_part, Handler &&h) {

    register_url_part<HTTP_VERB::HttpVerbDELETE>(url_part,
                                                 std::forward<Handler>(h));
  }

  void start() { receive_next_request(); }

private:
  std::wstring format_url_base(std::wstring base_url) {
    // ensure the URL starts w/ http:// or https://
    // ensure does not finish w/ '/'
    if (base_url.size() < 7)
      throw std::exception("invalid argument base_url");
    if (base_url[base_url.size() - 1] == L'/')
      base_url.pop_back();
    return base_url;
  }
  void validate_url_part(const std::wstring &url_part) {
    if (url_part.size() < 1 || url_part[0] != L'/')
      throw std::exception("invalid argument url_part");
  }
  std::wstring build_url(const std::wstring &url_part) {
    return base_url_ + url_part;
  }

  template <HTTP_VERB verb, typename Handler>
  void register_url_part(const std::wstring &url_part, Handler &&h) {
    validate_url_part(url_part);
    register_handler<verb>(build_url(url_part), std::forward<Handler>(h));
  }

  template <HTTP_VERB verb, typename Handler>
  void register_handler(const std::wstring &url, Handler &&h) {
    handlers_[url][verb] = std::forward<Handler>(h);
  }

  void receive_next_request() {
    auto rq = std::make_shared<request_response>();
    http::async_receive(
        queue_, rq->first.get_request_dynamic_buffer(),
        [this, rq](const boost::system::error_code &ec, size_t) {
          receive_next_request();
          if (ec)
            return;
          http::async_receive_body(
              queue_, rq->first.get_request_id(),
              rq->first.get_body_dynamic_buffer(),
              [this, rq](const boost::system::error_code &ec, size_t) {
                if (!ec)
                  dispatch(*rq);
              });
        });
  }

  void dispatch(request_response &rq) {

    auto *prq = rq.first.get_request();
    const auto *url_b = prq->CookedUrl.pFullUrl;
    const auto *url_e =
        prq->CookedUrl.pQueryString == nullptr
            ? prq->CookedUrl.pFullUrl + (prq->CookedUrl.FullUrlLength /
                                         sizeof(std::wstring::value_type))
            : prq->CookedUrl.pQueryString;

    std::wstring url(url_b, url_e);

    if (handlers_.find(url) == handlers_.end() ||
        handlers_[url].at(prq->Verb) == nullptr) {
      rq.second.set_status_code(404);
      rq.second.set_reason("Not found");
    } else {
      rq.second.set_status_code(200); // default to 200
      handlers_[url].at(prq->Verb)(rq.first, rq.second);
    }
    queue_.async_send_response(
        rq.second.get_response(), rq.first.get_request_id(),
        HTTP_SEND_RESPONSE_FLAG_DISCONNECT,
        [rq](const boost::system::error_code &ec, size_t) {});
  }

private:
  std::map<std::wstring,
           std::array<request_handler, HTTP_VERB::HttpVerbMaximum>>
      handlers_;
  const std::wstring base_url_;
  basic_http_handle<Executor> &queue_;
};

#pragma pop_macro("DELETE")

} // namespace http
} // namespace winasio
} // namespace boost