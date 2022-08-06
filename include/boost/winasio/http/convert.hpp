#pragma once

#include "http.h"
#include <map> // for headers

namespace boost {
namespace winasio {
namespace http {

// convert asio mutable buffer to request
template <typename MutableBufferSequence>
PHTTP_REQUEST phttp_request(const MutableBufferSequence &buffer) {
  return (PHTTP_REQUEST)buffer.data();
}

// buffer type is typically vector<byte>
template <typename BufferType> class simple_request {
public:
  simple_request()
      : request_buffer_(sizeof(HTTP_REQUEST) + 2048, 0), body_buffer_(1024) {
    HTTP_SET_NULL_ID(&(this->get_request()->RequestId));
  }

  BufferType &get_request_buffer() { return this->request_buffer_; }

  const BufferType &get_body_buffer() const { return this->body_buffer_; }

  PHTTP_REQUEST get_request() const { return phttp_request(request_buffer_); }

  HTTP_REQUEST_ID get_request_id() { return this->get_request()->RequestId; }

  // set body length of the buffer that is written
  void set_body_len(std::size_t len) { body_len_ = len; }

  // return body as string
  std::string get_body_string() const {
    return std::string(this->get_body_buffer().begin(),
                       this->get_body_buffer().begin() + this->body_len_);
  }

  std::map<HTTP_HEADER_ID, std::string> get_known_headers() const {
    std::map<HTTP_HEADER_ID, std::string> headers_map;
    PHTTP_REQUEST req = this->get_request();
    PHTTP_KNOWN_HEADER known_headers = req->Headers.KnownHeaders;
    size_t count = sizeof(req->Headers.KnownHeaders) /
                   sizeof(req->Headers.KnownHeaders[0]);
    for (int i = 0; i < count; i++) {
      HTTP_KNOWN_HEADER &header = known_headers[i];
      if (header.RawValueLength == 0) {
        continue;
      }
      headers_map[static_cast<HTTP_HEADER_ID>(i)] = std::string(
          header.pRawValue, header.pRawValue + header.RawValueLength);
    }
    return headers_map;
  }

  std::map<std::string, std::string> get_unknown_headers() const {
    std::map<std::string, std::string> headers_map;
    PHTTP_REQUEST req = this->get_request();
    size_t count = req->Headers.UnknownHeaderCount;
    if (count == 0) {
      return headers_map;
    }
    PHTTP_UNKNOWN_HEADER unknown_headers = req->Headers.pUnknownHeaders;
    for (int i = 0; i < count; i++) {
      HTTP_UNKNOWN_HEADER &header = unknown_headers[i];
      headers_map[std::string(header.pName, header.pName + header.NameLength)] =
          std::string(header.pRawValue,
                      header.pRawValue + header.RawValueLength);
    }
    return headers_map;
  }

private:
  BufferType request_buffer_; // buffer that backs request
  BufferType body_buffer_;    // buffer to hold body
  std::size_t body_len_;
};

template <typename BufferType>
std::ostream &operator<<(std::ostream &os,
                         simple_request<BufferType> const &m) {
  // return os << m.i;
  std::map<HTTP_HEADER_ID, std::string> known_headers = m.get_known_headers();
  for (auto const &x : known_headers) {
    os << "KnonwHeader [" << x.first << "] " << x.second << "\n";
  }
  std::map<std::string, std::string> unknown_headers = m.get_unknown_headers();
  for (auto const &x : unknown_headers) {
    os << x.first << " " << x.second << "\n";
  }
  os << m.get_body_string() << "\n";
  return os;
}

// buffer type is typically std::string
template <typename BufferType> class simple_response {
public:
  simple_response()
      : resp_(), data_chunks_(), reason_(), body_(), known_headers_(),
        unknown_headers_() {}

  void set_reason(const std::string &reason) { reason_ = reason; }

  void set_content_type(const std::string &content_type) {
    // content_type_ = content_type;
    this->add_known_header(HttpHeaderContentType, content_type);
  }

  void set_status_code(USHORT status_code) { status_code_ = status_code; }

  void set_body(const BufferType &body) { body_ = body; }

  void add_known_header(HTTP_HEADER_ID id, std::string data) {
    this->known_headers_[id] = data;
  }

  void add_unknown_header(std::string name, std::string val) {
    this->unknown_headers_[name] = val;
  }

  void add_trailer(std::string name, std::string val) {
    this->trailers_[name] = val;
  }

  PHTTP_RESPONSE get_response() {
    // prepare response and then return ther ptr
    resp_.StatusCode = status_code_;
    resp_.pReason = reason_.data();
    resp_.ReasonLength = static_cast<USHORT>(reason_.size());

    //
    // Add a known header.
    //
    for (auto const &x : known_headers_) {
      resp_.Headers.KnownHeaders[static_cast<int>(x.first)].pRawValue =
          x.second.c_str();
      resp_.Headers.KnownHeaders[static_cast<int>(x.first)].RawValueLength =
          static_cast<USHORT>(x.second.size());
    }

    unknown_headers_buff_.clear();
    for (auto const &x : unknown_headers_) {
      HTTP_UNKNOWN_HEADER header;
      header.pName = x.first.c_str();
      header.NameLength = static_cast<USHORT>(x.first.size());
      header.pRawValue = x.second.c_str();
      header.RawValueLength = static_cast<USHORT>(x.second.size());
      unknown_headers_buff_.push_back(std::move(header));
    }

    if (unknown_headers_buff_.size() > 0) {
      resp_.Headers.UnknownHeaderCount =
          static_cast<USHORT>(unknown_headers_buff_.size());
      resp_.Headers.pUnknownHeaders = unknown_headers_buff_.data();
    }

    if (body_.size() > 0) {
      //
      // Add an entity chunk.
      //
      HTTP_DATA_CHUNK data_chunk;
      data_chunk.DataChunkType = HttpDataChunkFromMemory;
      data_chunk.FromMemory.pBuffer = (PVOID)body_.c_str();
      data_chunk.FromMemory.BufferLength = (ULONG)body_.size();
      data_chunks_.push_back(std::move(data_chunk));
    }

    // prepare trailers
    trailers_buff_.clear();
    for (auto const &x : trailers_) {
      HTTP_UNKNOWN_HEADER header;
      header.pName = x.first.c_str();
      header.NameLength = static_cast<USHORT>(x.first.size());
      header.pRawValue = x.second.c_str();
      header.RawValueLength = static_cast<USHORT>(x.second.size());
      trailers_buff_.push_back(std::move(header));
    }
    if (trailers_buff_.size() > 0) {
      HTTP_DATA_CHUNK data_chunk;
      data_chunk.DataChunkType = HttpDataChunkTrailers;
      data_chunk.Trailers.TrailerCount =
          static_cast<USHORT>(trailers_buff_.size());
      data_chunk.Trailers.pTrailers = trailers_buff_.data();
      data_chunks_.push_back(std::move(data_chunk));
    }

    if (data_chunks_.size() > 0) {
      resp_.EntityChunkCount = static_cast<USHORT>(data_chunks_.size());
      resp_.pEntityChunks = data_chunks_.data();
    }

    return &this->resp_;
  }

private:
  HTTP_RESPONSE resp_;
  std::vector<HTTP_DATA_CHUNK> data_chunks_;
  std::string reason_;
  USHORT status_code_;
  BufferType body_;
  std::map<HTTP_HEADER_ID, std::string> known_headers_;
  std::map<std::string, std::string>
      unknown_headers_; // this takes ownership of string
  std::vector<HTTP_UNKNOWN_HEADER>
      unknown_headers_buff_; // this is the array needed in payload

  std::map<std::string, std::string> trailers_;
  std::vector<HTTP_UNKNOWN_HEADER> trailers_buff_;
};
} // namespace http
} // namespace winasio
} // namespace boost