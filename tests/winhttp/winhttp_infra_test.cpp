//
// Copyright (c) 2022 Youyuan Wu
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/ut.hpp>

#include "boost/asio.hpp"
#include "boost/winasio/winhttp/winhttp.hpp"
// include temp impl/application of winhttp
// #include "boost\winasio\winhttp\temp.hpp"

#include <iostream>

namespace net = boost::asio; // from <boost/asio.hpp>
namespace winnet = boost::winasio;

boost::ut::suite errors = [] {
  using namespace boost::ut;

  "CrackURL"_test = [] {
    boost::system::error_code ec;
    winnet::winhttp::url_component x;
    x.crack(L"https://www.google.com:12345/person?name=john", ec);
    boost::ut::expect(!ec.failed());
    boost::ut::expect(std::wstring(L"www.google.com") == x.get_hostname());
    boost::ut::expect(INTERNET_SCHEME_HTTPS == x.get_nscheme());
    boost::ut::expect(12345 == x.get_port());
    boost::ut::expect(std::wstring(L"/person") == x.get_path());
    boost::ut::expect(std::wstring(L"?name=john") == x.get_query());
  };

  // exercise for using buffer.
  "ASIOBuff"_test = [] {
    std::vector<BYTE> data(0);
    auto buff = net::dynamic_buffer(data);

    boost::ut::expect(0 == buff.size());
    auto part = buff.prepare(5);
    std::size_t n = net::buffer_copy(part, net::const_buffer("hello", 5));
    boost::ut::expect(5 == n);
    boost::ut::expect('h' == data[0]);
    boost::ut::expect(5 == data.size());

    boost::ut::expect(0 == buff.size()); // not commited.
    buff.commit(n);
    boost::ut::expect(5 == buff.size());
  };

  "AcceptType"_test = [] {
    winnet::winhttp::header::accept_types at = {L"application/json"};
    const LPCWSTR *data = at.get();
    bool str_eq = std::wstring(*data) == std::wstring(L"application/json");
    boost::ut::expect(str_eq);
    boost::ut::expect(*(data + 1) == nullptr);
  };

  "AdditionalHeaders"_test = [] {
    winnet::winhttp::header::headers hs;
    hs.add(L"myheader1", L"myval1").add(L"myheader2", L"myval2");

    bool str_eq = std::wstring(L"myheader1: myval1\r\nmyheader2: myval2") ==
                  std::wstring(hs.get());
    boost::ut::expect(str_eq);

    winnet::winhttp::header::headers hs0; // empty
    boost::ut::expect(nullptr == hs0.get());
    boost::ut::expect(0u == hs0.size());
  };
}; // namespace

int main() {}