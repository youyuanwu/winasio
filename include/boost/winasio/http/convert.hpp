#pragma once

#include "boost/winasio/http/http_asio.hpp"

#include <map> // for headers

namespace boost
{
    namespace winasio
    {
        namespace http
        {

            inline void
            get_known_headers_all(PHTTP_REQUEST req, std::map<HTTP_HEADER_ID, std::string> & headers)
            {
                PHTTP_KNOWN_HEADER known_headers = req->Headers.KnownHeaders;
                size_t count =
                    sizeof(req->Headers.KnownHeaders) / sizeof(req->Headers.KnownHeaders[0]);
                for (size_t i = 0; i < count; ++i)
                {
                    HTTP_KNOWN_HEADER & header = known_headers[i];
                    if (header.RawValueLength == 0)
                    {
                        continue;
                    }
                    headers[static_cast<HTTP_HEADER_ID>(i)] =
                        std::string(header.pRawValue, header.pRawValue + header.RawValueLength);
                }
            }

            // query a paticular known header.
            // return true if found
            inline bool query_known_header(PHTTP_REQUEST req, HTTP_HEADER_ID id, std::string & val)
            {
                PHTTP_KNOWN_HEADER known_headers = req->Headers.KnownHeaders;
                HTTP_KNOWN_HEADER & header = known_headers[id];
                if (header.RawValueLength == 0)
                {
                    return false;
                }
                val = std::string(header.pRawValue, header.pRawValue + header.RawValueLength);
                return true;
            }

            inline void
            get_unknown_headers_all(PHTTP_REQUEST req, std::map<std::string, std::string> & headers)
            {
                size_t count = req->Headers.UnknownHeaderCount;
                if (count == 0)
                {
                    return;
                }
                PHTTP_UNKNOWN_HEADER unknown_headers = req->Headers.pUnknownHeaders;
                for (size_t i = 0; i < count; ++i)
                {
                    HTTP_UNKNOWN_HEADER & header = unknown_headers[i];
                    headers[std::string(header.pName, header.pName + header.NameLength)] =
                        std::string(header.pRawValue, header.pRawValue + header.RawValueLength);
                }
            }

            class simple_request
            {
            public:
                simple_request()
                    : // request_buffer_(sizeof(HTTP_REQUEST) + 2048, 0),
                    request_buffer_(0, 0)
                    , body_buffer_(0, 0)
                    , dynamic_request_buff_(request_buffer_)
                    , dynamic_body_buff_(body_buffer_)
                {
                }

                inline auto & get_request_dynamic_buffer()
                {
                    return this->dynamic_request_buff_;
                }

                inline auto & get_body_dynamic_buffer() { return this->dynamic_body_buff_; }

                // const BufferType &get_body_buffer() const { return this->body_buffer_; }

                inline PHTTP_REQUEST get_request() const
                {
                    auto buff = this->dynamic_request_buff_.data();
                    return phttp_request(buff);
                }

                inline HTTP_REQUEST_ID get_request_id() const
                {
                    return this->get_request()->RequestId;
                }

                // return body as string
                inline std::string get_body_string() const
                {
                    auto body = dynamic_body_buff_.data();
                    auto view = body.data();
                    auto size = body.size();
                    return std::string((BYTE *) view, (BYTE *) view + size);
                }

            private:
                std::vector<CHAR> request_buffer_; // buffer that backs request
                net::dynamic_vector_buffer<CHAR, std::allocator<CHAR>>
                    dynamic_request_buff_;      // dynamic buff wrapper for request
                std::vector<CHAR> body_buffer_; // buffer to hold body
                net::dynamic_vector_buffer<CHAR, std::allocator<CHAR>> dynamic_body_buff_;
            };

            inline std::ostream & operator<<(std::ostream & os, simple_request const & m)
            {

                PHTTP_REQUEST req = m.get_request();
                std::map<HTTP_HEADER_ID, std::string> known_headers;
                get_known_headers_all(req, known_headers);
                for (auto const & x : known_headers)
                {
                    os << "KnonwHeader [" << x.first << "] " << x.second << "\n";
                }

                std::map<std::string, std::string> unknown_headers;
                get_unknown_headers_all(req, unknown_headers);

                for (auto const & x : unknown_headers)
                {
                    os << x.first << " " << x.second << "\n";
                }
                os << m.get_body_string() << "\n";
                return os;
            }

            class simple_response
            {
            public:
                simple_response()
                    : resp_()
                    , data_chunks_()
                    , reason_()
                    , body_()
                    , known_headers_()
                    , unknown_headers_()
                {
                }

                inline void set_reason(const std::string & reason) { reason_ = reason; }

                inline void set_content_type(const std::string & content_type)
                {
                    // content_type_ = content_type;
                    this->add_known_header(HttpHeaderContentType, content_type);
                }

                inline void set_status_code(USHORT status_code)
                {
                    status_code_ = status_code;
                }

                inline void set_body(const std::string & body) { body_ = body; }

                inline void add_known_header(HTTP_HEADER_ID id, std::string data)
                {
                    this->known_headers_[id] = data;
                }

                inline void add_unknown_header(std::string name, std::string val)
                {
                    this->unknown_headers_[name] = val;
                }

                inline void add_trailer(std::string name, std::string val)
                {
                    this->trailers_[name] = val;
                }

                inline PHTTP_RESPONSE get_response()
                {
                    // prepare response and then return ther ptr
                    resp_.StatusCode = status_code_;
                    resp_.pReason = reason_.data();
                    resp_.ReasonLength = static_cast<USHORT>(reason_.size());

                    //
                    // Add a known header.
                    //
                    for (auto const & x : known_headers_)
                    {
                        resp_.Headers.KnownHeaders[static_cast<int>(x.first)].pRawValue =
                            x.second.c_str();
                        resp_.Headers.KnownHeaders[static_cast<int>(x.first)].RawValueLength =
                            static_cast<USHORT>(x.second.size());
                    }

                    unknown_headers_buff_.clear();
                    for (auto const & x : unknown_headers_)
                    {
                        HTTP_UNKNOWN_HEADER header;
                        header.pName = x.first.c_str();
                        header.NameLength = static_cast<USHORT>(x.first.size());
                        header.pRawValue = x.second.c_str();
                        header.RawValueLength = static_cast<USHORT>(x.second.size());
                        unknown_headers_buff_.push_back(std::move(header));
                    }

                    if (unknown_headers_buff_.size() > 0)
                    {
                        resp_.Headers.UnknownHeaderCount =
                            static_cast<USHORT>(unknown_headers_buff_.size());
                        resp_.Headers.pUnknownHeaders = unknown_headers_buff_.data();
                    }

                    if (body_.size() > 0)
                    {
                        //
                        // Add an entity chunk.
                        //
                        HTTP_DATA_CHUNK data_chunk;
                        data_chunk.DataChunkType = HttpDataChunkFromMemory;
                        data_chunk.FromMemory.pBuffer = (PVOID) body_.c_str();
                        data_chunk.FromMemory.BufferLength = (ULONG) body_.size();
                        data_chunks_.push_back(std::move(data_chunk));
                    }

                    // prepare trailers
                    trailers_buff_.clear();
                    for (auto const & x : trailers_)
                    {
                        HTTP_UNKNOWN_HEADER header;
                        header.pName = x.first.c_str();
                        header.NameLength = static_cast<USHORT>(x.first.size());
                        header.pRawValue = x.second.c_str();
                        header.RawValueLength = static_cast<USHORT>(x.second.size());
                        trailers_buff_.push_back(std::move(header));
                    }
                    if (trailers_buff_.size() > 0)
                    {
                        HTTP_DATA_CHUNK data_chunk;
                        data_chunk.DataChunkType = HttpDataChunkTrailers;
                        data_chunk.Trailers.TrailerCount =
                            static_cast<USHORT>(trailers_buff_.size());
                        data_chunk.Trailers.pTrailers = trailers_buff_.data();
                        data_chunks_.push_back(std::move(data_chunk));
                    }

                    if (data_chunks_.size() > 0)
                    {
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
                std::string body_;
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
