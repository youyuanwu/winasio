#pragma once

// This is abandoned code and should not be used

// temporary implementations.
namespace boost {
namespace winasio {
namespace winhttp {

namespace net = boost::asio;
namespace winnet = boost::winasio;

template <typename DynamicBuffer>
// Context value structure.
class REQUEST_CONTEXT {
public:
  template <typename Executor>
  REQUEST_CONTEXT(const Executor &ex, DynamicBuffer &buff)
      : h_request(ex), buff_(buff) {}

  // record the final request err in context and finish request.
  void cleanup(_In_ const boost::system::error_code &err) {
    // record final error
    this->ec = err;

    boost::system::error_code ec;
    // Set the memo to indicate a closed handle.
#ifdef WINASIO_LOG
    BOOST_LOG_TRIVIAL(debug) << "Cleanup";
#endif
    // ignore error code
    this->h_request.set_status_callback(NULL, ec);
    ec.clear();

    // notify asio request completes.
    // This is broken already by new version.
    // this->h_request.complete();
  }

  bool header(_Out_ boost::system::error_code &ec) {
    std::wstring all_headers;

    header::get_all_raw_crlf(this->h_request, ec, all_headers);

    if (!ec) {
      BOOST_LOG_TRIVIAL(debug) << all_headers;
    }
    return !ec.failed();
  }

  bool query_data(_Out_ boost::system::error_code &ec) {
    this->h_request.query_data_available(NULL, ec);
    return !ec.failed();
  }

  void on_read_complete(DWORD dwBytesRead) { this->buff_.commit(dwBytesRead); }

  // perform read of data
  bool on_data_available(const DWORD len, _Out_ boost::system::error_code &ec) {
    auto buff = this->buff_.prepare(len + 1);
    // Read the available data.
    this->h_request.read_data((LPVOID)buff.data(), len, NULL, ec);
    return !ec.failed();
  }

  basic_winhttp_request_handle<net::io_context::executor_type> h_request;

  DynamicBuffer &buff_;
  boost::system::error_code ec; // final error from request
};

template <typename DynamicBuffer>
void __stdcall AsyncCallback(HINTERNET hInternet, DWORD_PTR dwContext,
                             DWORD dwInternetStatus,
                             LPVOID lpvStatusInformation,
                             DWORD dwStatusInformationLength) {
  REQUEST_CONTEXT<DynamicBuffer> *cpContext;
  cpContext = (REQUEST_CONTEXT<DynamicBuffer> *)dwContext;

  boost::system::error_code ec;

  if (cpContext == NULL) {
    // this should not happen, but we are being defensive here
    return;
  }

  // Create a string that reflects the status flag.
  switch (dwInternetStatus) {
  case WINHTTP_CALLBACK_STATUS_CLOSING_CONNECTION:
    // Closing the connection to the server.The lpvStatusInformation parameter
    // is NULL.
    BOOST_LOG_TRIVIAL(debug)
        << L"CLOSING_CONNECTION " << dwStatusInformationLength;
    break;

  case WINHTTP_CALLBACK_STATUS_CONNECTED_TO_SERVER:
    // Successfully connected to the server.
    // The lpvStatusInformation parameter contains a pointer to an LPWSTR that
    // indicates the IP address of the server in dotted notation.
    if (lpvStatusInformation) {
      BOOST_LOG_TRIVIAL(debug)
          << L"CONNECTED_TO_SERVER " << (WCHAR *)lpvStatusInformation;
    } else {
      BOOST_LOG_TRIVIAL(debug)
          << L"CONNECTED_TO_SERVER " << dwStatusInformationLength;
    }
    break;

  case WINHTTP_CALLBACK_STATUS_CONNECTING_TO_SERVER:
    // Connecting to the server.
    // The lpvStatusInformation parameter contains a pointer to an LPWSTR that
    // indicates the IP address of the server in dotted notation.
    if (lpvStatusInformation) {
      BOOST_LOG_TRIVIAL(debug)
          << L"CONNECTING_TO_SERVER " << (WCHAR *)lpvStatusInformation;
    } else {
      BOOST_LOG_TRIVIAL(debug)
          << L"CONNECTING_TO_SERVER " << dwStatusInformationLength;
    }
    break;

  case WINHTTP_CALLBACK_STATUS_CONNECTION_CLOSED:
    // Successfully closed the connection to the server. The
    // lpvStatusInformation parameter is NULL.
    BOOST_LOG_TRIVIAL(debug)
        << L"CONNECTION_CLOSED " << dwStatusInformationLength;
    break;

  case WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE: {
    // Data is available to be retrieved with WinHttpReadData.The
    // lpvStatusInformation parameter points to a DWORD that contains the number
    // of bytes of data available. The dwStatusInformationLength parameter
    // itself is 4 (the size of a DWORD).

    BOOST_ASSERT(dwStatusInformationLength == sizeof(DWORD));
    DWORD data_len = *((LPDWORD)lpvStatusInformation);

    // If there is no data, the process is complete.
    if (data_len == 0) {
      BOOST_LOG_TRIVIAL(debug)
          << "DATA_AVAILABLE Number of bytes available :" << data_len
          << " All data has "
             "been read -> End request.";
      // All of the data has been read.  End request
      // Close the request and connect handles for this context.
      cpContext->cleanup(ec);
    } else {
      BOOST_LOG_TRIVIAL(debug) << "Read Next block: size=" << data_len;
      // Otherwise, read the next block of data.
      if (cpContext->on_data_available(data_len, ec) == FALSE) {
        cpContext->cleanup(ec);
      }
    }
  } break;

  case WINHTTP_CALLBACK_STATUS_HANDLE_CREATED:
    // An HINTERNET handle has been created. The lpvStatusInformation parameter
    // contains a pointer to the HINTERNET handle.
    if (lpvStatusInformation) {
      BOOST_LOG_TRIVIAL(debug)
          << L"HANDLE_CREATED : "
          << static_cast<LPHINTERNET>(lpvStatusInformation);
    } else {
      BOOST_LOG_TRIVIAL(debug)
          << L"HANDLE_CREATED " << dwStatusInformationLength;
    }
    break;

  case WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING:
    // This handle value has been terminated. The lpvStatusInformation parameter
    // contains a pointer to the HINTERNET handle. There will be no more
    // callbacks for this handle.
    if (lpvStatusInformation) {
      BOOST_LOG_TRIVIAL(debug)
          << L"HANDLE_CLOSING : "
          << static_cast<LPHINTERNET>(lpvStatusInformation);
    } else {
      BOOST_LOG_TRIVIAL(debug)
          << L"HANDLE_CLOSING " << dwStatusInformationLength;
    }
    break;

  case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE:
    // The response header has been received and is available with
    // WinHttpQueryHeaders. The lpvStatusInformation parameter is NULL.
    BOOST_LOG_TRIVIAL(debug)
        << L"HEADERS_AVAILABLE " << dwStatusInformationLength;
    cpContext->header(ec); // ignore ec?
    ec.clear();

    // Begin downloading the resource.
    if (cpContext->query_data(ec) == FALSE) {
      cpContext->cleanup(ec);
    }
    break;

  case WINHTTP_CALLBACK_STATUS_INTERMEDIATE_RESPONSE:
    // Received an intermediate (100 level) status code message from the server.
    // The lpvStatusInformation parameter contains a pointer to a DWORD that
    // indicates the status code.
    if (lpvStatusInformation) {
      BOOST_LOG_TRIVIAL(debug) << L"INTERMEDIATE_RESPONSE Status code : "
                               << *(DWORD *)lpvStatusInformation;
    } else {
      BOOST_LOG_TRIVIAL(debug)
          << L"INTERMEDIATE_RESPONSE " << dwStatusInformationLength;
    }
    break;

  case WINHTTP_CALLBACK_STATUS_NAME_RESOLVED:
    // Successfully found the IP address of the server. The lpvStatusInformation
    // parameter contains a pointer to a LPWSTR that indicates the name that was
    // resolved.
    if (lpvStatusInformation) {
      BOOST_LOG_TRIVIAL(debug) << L"NAME_RESOLVED : " << lpvStatusInformation;
    } else {
      BOOST_LOG_TRIVIAL(debug)
          << L"NAME_RESOLVED " << dwStatusInformationLength;
    }
    break;

  case WINHTTP_CALLBACK_STATUS_READ_COMPLETE:
    // Data was successfully read from the server. The lpvStatusInformation
    // parameter contains a pointer to the buffer specified in the call to
    // WinHttpReadData. The dwStatusInformationLength parameter contains the
    // number of bytes read. When used by WinHttpWebSocketReceive, the
    // lpvStatusInformation parameter contains a pointer to a
    // WINHTTP_WEB_SOCKET_STATUS structure, 	and the
    // dwStatusInformationLength
    // parameter indicates the size of lpvStatusInformation.

    BOOST_LOG_TRIVIAL(debug)
        << "READ_COMPLETE Number of bytes read" << dwStatusInformationLength;
    // Copy the data and delete the buffers.

    if (dwStatusInformationLength != 0) {
      cpContext->on_read_complete(dwStatusInformationLength);

      // Check for more data.
      if (cpContext->query_data(ec) == FALSE) {
        cpContext->cleanup(ec);
      }
    }
    break;

  case WINHTTP_CALLBACK_STATUS_RECEIVING_RESPONSE:
    // Waiting for the server to respond to a request. The lpvStatusInformation
    // parameter is NULL.
    BOOST_LOG_TRIVIAL(debug)
        << "RECEIVING_RESPONSE " << dwStatusInformationLength;
    break;

  case WINHTTP_CALLBACK_STATUS_REDIRECT:
    // An HTTP request is about to automatically redirect the request. The
    // lpvStatusInformation parameter contains a pointer to an LPWSTR indicating
    // the new URL. At this point, the application can read any data returned by
    // the server with the redirect response and can query the response headers.
    // It can also cancel the operation by closing the handle

    if (lpvStatusInformation) {
      BOOST_LOG_TRIVIAL(debug)
          << "REDIRECT to " << (WCHAR *)lpvStatusInformation;
    } else {
      BOOST_LOG_TRIVIAL(debug) << "REDIRECT " << dwStatusInformationLength;
    }
    break;

  case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR: {
    // An error occurred while sending an HTTP request.
    // The lpvStatusInformation parameter contains a pointer to a
    // WINHTTP_ASYNC_RESULT structure. Its dwResult member indicates the ID of
    // the called function and dwError indicates the return value.
    WINHTTP_ASYNC_RESULT *pAR = (WINHTTP_ASYNC_RESULT *)lpvStatusInformation;

    std::wstring err;
    winnet::winhttp::error::get_api_error_str(pAR, err);
    BOOST_LOG_TRIVIAL(debug) << err;
    // Error ERROR_INTERNET_NAME_NOT_RESOLVED
    // if (pAR->dwError == 0x12027) {
    //   // proxy name not resolved or host name not resolved
    // }
    ec.assign(pAR->dwError, boost::asio::error::get_system_category());
    cpContext->cleanup(ec);
  } break;

  case WINHTTP_CALLBACK_STATUS_REQUEST_SENT:
    // Successfully sent the information request to the server.
    // The lpvStatusInformation parameter contains a pointer to a DWORD
    // indicating the number of bytes sent.
    if (lpvStatusInformation) {
      BOOST_LOG_TRIVIAL(debug) << "REQUEST_SENT Number of bytes sent : "
                               << *(DWORD *)lpvStatusInformation;
    } else {
      BOOST_LOG_TRIVIAL(debug) << "REQUEST_SENT " << dwStatusInformationLength;
    }
    break;

  case WINHTTP_CALLBACK_STATUS_RESOLVING_NAME:
    // Looking up the IP address of a server name. The lpvStatusInformation
    // parameter contains a pointer to the server name being resolved.
    if (lpvStatusInformation) {
      BOOST_LOG_TRIVIAL(debug)
          << L"RESOLVING_NAME " << (WCHAR *)lpvStatusInformation;
    } else {
      BOOST_LOG_TRIVIAL(debug)
          << L"RESOLVING_NAME " << dwStatusInformationLength;
    }
    break;

  case WINHTTP_CALLBACK_STATUS_RESPONSE_RECEIVED:
    // Successfully received a response from the server.
    // The lpvStatusInformation parameter contains a pointer to a DWORD
    // indicating the number of bytes received.
    if (lpvStatusInformation) {
      BOOST_LOG_TRIVIAL(debug) << L"RESPONSE_RECEIVED. Number of bytes : %d"
                               << *(DWORD *)lpvStatusInformation;
    } else {
      BOOST_LOG_TRIVIAL(debug)
          << L"RESPONSE_RECEIVED " << dwStatusInformationLength;
    }
    break;

  case WINHTTP_CALLBACK_STATUS_SECURE_FAILURE: {
    // One or more errors were encountered while retrieving a Secure Sockets
    // Layer (SSL) certificate from the server.
    if (lpvStatusInformation) {
      std::wstring err;
      winnet::winhttp::error::get_secure_failure_err_str(
          *(DWORD *)lpvStatusInformation, err);
      BOOST_LOG_TRIVIAL(debug) << err;
    } else {
      BOOST_LOG_TRIVIAL(debug)
          << L"SECURE_FAILURE " << dwStatusInformationLength;
    }
  } break;

  case WINHTTP_CALLBACK_STATUS_SENDING_REQUEST:
    // Sending the information request to the server.The lpvStatusInformation
    // parameter is NULL.
    BOOST_LOG_TRIVIAL(debug)
        << L"SENDING_REQUEST " << dwStatusInformationLength;
    break;

  case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:
    BOOST_LOG_TRIVIAL(debug)
        << L"SENDREQUEST_COMPLETE " << dwStatusInformationLength;

    // Prepare the request handle to receive a response.
    cpContext->h_request.receive_response(ec);
    if (ec) {
      cpContext->cleanup(ec);
    }
    break;

  case WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE:
    // Data was successfully written to the server. The lpvStatusInformation
    // parameter contains a pointer to a DWORD that indicates the number of
    // bytes written. When used by WinHttpWebSocketSend, the
    // lpvStatusInformation parameter contains a pointer to a
    // WINHTTP_WEB_SOCKET_STATUS structure, and the dwStatusInformationLength
    // parameter indicates the size of lpvStatusInformation.
    if (lpvStatusInformation) {
      BOOST_LOG_TRIVIAL(debug) << L"WRITE_COMPLETE, bytes written: "
                               << *(DWORD *)lpvStatusInformation;
    } else {
      BOOST_LOG_TRIVIAL(debug)
          << L"WRITE_COMPLETE (%d)" << dwStatusInformationLength;
    }
    break;

  case WINHTTP_CALLBACK_STATUS_GETPROXYFORURL_COMPLETE:
    // The operation initiated by a call to WinHttpGetProxyForUrlEx is complete.
    // Data is available to be retrieved with WinHttpReadData.
    BOOST_LOG_TRIVIAL(debug)
        << L"GETPROXYFORURL_COMPLETE" << dwStatusInformationLength;
    break;

  case WINHTTP_CALLBACK_STATUS_CLOSE_COMPLETE:
    // The connection was successfully closed via a call to
    // WinHttpWebSocketClose.
    BOOST_LOG_TRIVIAL(debug) << L"CLOSE_COMPLETE " << dwStatusInformationLength;
    break;

  case WINHTTP_CALLBACK_STATUS_SHUTDOWN_COMPLETE:
    // The connection was successfully shut down via a call to
    // WinHttpWebSocketShutdown
    BOOST_LOG_TRIVIAL(debug)
        << L"SHUTDOWN_COMPLETE " << dwStatusInformationLength;
    break;

  default:
    BOOST_LOG_TRIVIAL(debug) << L"Unknown/unhandled callback - status "
                             << dwInternetStatus << L"given";
    break;
  }
}

} // namespace winhttp
} // namespace winasio
} // namespace boost