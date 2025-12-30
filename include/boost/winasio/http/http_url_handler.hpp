//
// Copyright (c) 2022 Youyuan Wu
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_WINASIO_HTTP_URL_HANDLER_HPP
#define BOOST_WINASIO_HTTP_URL_HANDLER_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
#pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <boost/asio/error.hpp>

#ifdef WINASIO_LOG
#include <spdlog/spdlog.h>
#endif

#include <boost/winasio/http/http_major_version.hpp>

#include <string>

namespace boost {
namespace winasio {
namespace http {

template <typename Executor, HTTP_MAJOR_VERSION> class url_handler {};

template <typename Executor>
class url_handler<Executor, HTTP_MAJOR_VERSION::http_ver_1> {
public:
  url_handler(basic_http_queue_handle<Executor> &queue_handle)
      : queue_handle_(queue_handle) {}
  void add_url(std::wstring const &url, system::error_code &ec) {
    DWORD retCode = HttpAddUrl(queue_handle_.native_handle(), // Req Queue
                               url.c_str(), // Fully qualified URL
                               NULL         // Reserved
    );
    // std::wcout << L"added url " << url << std::endl;
    ec = system::error_code(retCode, asio::error::get_system_category());
  }

  void remove_url(std::wstring const &url, system::error_code &ec) {
    DWORD retCode = HttpRemoveUrl(queue_handle_.native_handle(), // Req Queue
                                  url.c_str() // Fully qualified URL
    );
    // std::wcout << L"removed url " << url << std::endl;
    ec = system::error_code(retCode, asio::error::get_system_category());
  }

private:
  basic_http_queue_handle<Executor> &queue_handle_;
};

template <typename Executor>
class url_handler<Executor, HTTP_MAJOR_VERSION::http_ver_2> {
public:
  url_handler(basic_http_queue_handle<Executor> &queue_handle)
      : queue_handle_(queue_handle) {
    DWORD retCode = HttpCreateServerSession(HTTPAPI_VERSION_2, &session_id_, 0);
    if (retCode != NO_ERROR) {
#ifdef WINASIO_LOG
      spdlog::error("Failed to create session, err: {}", retCode);
#endif
      return;
    }
    retCode = HttpCreateUrlGroup(session_id_, &group_id_, 0);
    if (retCode != NO_ERROR) {
#ifdef WINASIO_LOG
      spdlog::error("Failed to create url group, err: {}", retCode);
#endif
      return;
    }
#ifdef WINASIO_LOG
    spdlog::debug("Successfully create url_session: {} , url_group: {}",
                  session_id_, group_id_);
#endif
    HTTP_BINDING_INFO binding_info{};
    binding_info.RequestQueueHandle = queue_handle_.native_handle();
    binding_info.Flags.Present = 1;
    retCode = HttpSetUrlGroupProperty(group_id_, HttpServerBindingProperty,
                                      &binding_info, sizeof(binding_info));
    if (retCode != NO_ERROR) {
#ifdef WINASIO_LOG
      spdlog::error("Failed to bind to request queue, err: {}", retCode);
#endif
    }
    BOOST_ASSERT(retCode == NO_ERROR);
  }
  ~url_handler() {
    DWORD retCode = NO_ERROR;
    if (group_id_) {
      retCode = HttpCloseUrlGroup(group_id_);
      BOOST_ASSERT(retCode == NO_ERROR);
    }
    if (session_id_) {
      retCode = HttpCloseServerSession(session_id_);
      BOOST_ASSERT(retCode == NO_ERROR);
    }
  }
  void add_url(std::wstring const &url, system::error_code &ec) {
    DWORD retCode = HttpAddUrlToUrlGroup(group_id_, url.c_str(), 0, 0);
    if (retCode != NO_ERROR) {
#ifdef WINASIO_LOG
      spdlog::error(L"Failed to add url: {}, err : {}", url, retCode);
#endif
    }
    ec = boost::system::error_code(retCode,
                                   boost::asio::error::get_system_category());
  }
  void remove_url(std::wstring const &url, boost::system::error_code &ec) {
    DWORD retCode = HttpRemoveUrlFromUrlGroup(group_id_, url.c_str(), 0);
    if (retCode != NO_ERROR) {
#ifdef WINASIO_LOG
      spdlog::error(L"Failed to remove url: {}, err : {}", url, retCode);
#endif
    }
    ec = boost::system::error_code(retCode,
                                   boost::asio::error::get_system_category());
  }
  HTTP_SERVER_SESSION_ID get_session_id() const { return session_id_; }
  HTTP_SERVER_SESSION_ID group_id() const { return group_id_; }

private:
  HTTP_SERVER_SESSION_ID session_id_ = 0;
  HTTP_URL_GROUP_ID group_id_ = 0;
  basic_http_queue_handle<Executor> &queue_handle_;
};

} // namespace http
} // namespace winasio
} // namespace boost

#endif // BOOST_WINASIO_HTTP_URL_HANDLER_HPP