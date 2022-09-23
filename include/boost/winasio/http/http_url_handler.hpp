#ifndef BOOST_WINASIO_HTTP_URL_HANDLER_HPP
#define BOOST_WINASIO_HTTP_URL_HANDLER_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
#pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <boost/asio/error.hpp>
#include <boost/log/trivial.hpp>
#include <boost/winasio/http/http_major_version.hpp>

#include <string>

namespace boost {
namespace winasio {
namespace http {

template <HTTP_MAJOR_VERSION> class url_handler {};

template <> class url_handler<HTTP_MAJOR_VERSION::http_ver_1> {
public:
  url_handler(HANDLE queue_handler) : queue_handler_(queue_handler) {}
  void add_url(std::wstring const &url, system::error_code &ec) {
    DWORD retCode = HttpAddUrl(queue_handler_, // Req Queue
                               url.c_str(),    // Fully qualified URL
                               NULL            // Reserved
    );
    // std::wcout << L"added url " << url << std::endl;
    ec = system::error_code(retCode, asio::error::get_system_category());
  }

  void remove_url(std::wstring const &url, system::error_code &ec) {
    DWORD retCode = HttpRemoveUrl(queue_handler_, // Req Queue
                                  url.c_str()     // Fully qualified URL
    );
    // std::wcout << L"removed url " << url << std::endl;
    ec = system::error_code(retCode, asio::error::get_system_category());
  }
  void set_queue_handler(HANDLE queue_handler, system::error_code &ec) {
    queue_handler_ = queue_handler;
  }

private:
  HANDLE queue_handler_ = nullptr;
};

template <> class url_handler<HTTP_MAJOR_VERSION::http_ver_2> {
public:
  url_handler(HANDLE queue_handler) : queue_handler_(queue_handler) {
    DWORD retCode = HttpCreateServerSession(HTTPAPI_VERSION_2, &session_id_, 0);
    if (retCode != NO_ERROR) {
      BOOST_LOG_TRIVIAL(error) << L"Failed to create session, err: " << retCode;
      return;
    }
    retCode = HttpCreateUrlGroup(session_id_, &group_id_, 0);
    if (retCode != NO_ERROR) {
      BOOST_LOG_TRIVIAL(error)
          << L"Failed to create url group, err: " << retCode;
      return;
    }
    BOOST_LOG_TRIVIAL(debug) << L"Successfully create url_session:"
                             << session_id_ << ", url_group:" << group_id_;
    HTTP_BINDING_INFO binding_info{};
    binding_info.RequestQueueHandle = queue_handler_;
    binding_info.Flags.Present = 1;
    retCode = HttpSetUrlGroupProperty(group_id_, HttpServerBindingProperty,
                                      &binding_info, sizeof(binding_info));
    if (retCode != NO_ERROR) {
      BOOST_LOG_TRIVIAL(error)
          << L"Failed to bind to request queue, err:" << retCode;
    }
  }
  ~url_handler() {
    if (group_id_) {
      HttpCloseUrlGroup(group_id_);
    }
    if (session_id_) {
      HttpCloseServerSession(session_id_);
    }
  }
  void add_url(std::wstring const &url, system::error_code &ec) {
    DWORD retCode = HttpAddUrlToUrlGroup(group_id_, url.c_str(), 0, 0);
    if (retCode != NO_ERROR) {
      BOOST_LOG_TRIVIAL(error)
          << L"Failed to add url: " << url << ", err : " << retCode;
    }
    ec = boost::system::error_code(retCode,
                                   boost::asio::error::get_system_category());
  }
  void remove_url(std::wstring const &url, boost::system::error_code &ec) {
    DWORD retCode = HttpRemoveUrlFromUrlGroup(group_id_, url.c_str(), 0);
    if (retCode != NO_ERROR) {
      BOOST_LOG_TRIVIAL(error)
          << L"Failed to remove url: " << url << ", err : " << retCode;
    }
    ec = boost::system::error_code(retCode,
                                   boost::asio::error::get_system_category());
  }
  HTTP_SERVER_SESSION_ID get_session_id() const { return session_id_; }
  HTTP_SERVER_SESSION_ID group_id() const { return group_id_; }

private:
  HTTP_SERVER_SESSION_ID session_id_ = 0;
  HTTP_URL_GROUP_ID group_id_ = 0;
  HANDLE queue_handler_ = nullptr;
};

} // namespace http
} // namespace winasio
} // namespace boost

#endif // BOOST_WINASIO_HTTP_URL_HANDLER_HPP