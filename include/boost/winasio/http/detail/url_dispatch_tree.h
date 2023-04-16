//
// Copyright (c) 2022 Youyuan Wu
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_WINASIO_HTTP_DETAIL_URL_DISPATCH_TREE_HPP
#define BOOST_WINASIO_HTTP_DETAIL_URL_DISPATCH_TREE_HPP

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace boost {
namespace winasio {
namespace http {
namespace detail {

template <typename CharT, size_t max_specifier, typename DispatchT>
class url_dispatch_tree {

public:
  using string_t = std::basic_string<CharT>;
  using parameters_t = std::map<string_t, string_t>;
  using dispatch_fn_t = std::function<void(const parameters_t &, DispatchT)>;

private:
  using fn_array_t = std::array<dispatch_fn_t, max_specifier>;

private:
  struct url_node {
    
    std::vector<url_node> next;
  };

public:
  template <typename HandlerT>
  void register_fn(const string_t &url, HandlerT &&on_url) {}

  bool dispatch(const string_t &url, size_t specifier, DispatchT value) {}

private:
  std::vector<url_node> roots_;
};

} // namespace detail
} // namespace http
} // namespace winasio
} // namespace boost

#endif // BOOST_WINASIO_HTTP_DETAIL_URL_DISPATCH_TREE_HPP