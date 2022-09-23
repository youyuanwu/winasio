#ifndef BOOST_WINASIO_HTTP_INITIALIZER_HPP
#define BOOST_WINASIO_HTTP_INITIALIZER_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
#pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <http.h>

namespace boost
{
    namespace winasio
    {
        namespace http
        {

            class http_initializer
            {
            public:
                http_initializer()
                {
                    DWORD retCode =
                        HttpInitialize(HTTPAPI_VERSION_1,
                                       HTTP_INITIALIZE_SERVER | HTTP_INITIALIZE_CONFIG, // Flags
                                       NULL                                             // Reserved
                        );
                    BOOST_ASSERT(retCode == NO_ERROR);
                }
                ~http_initializer()
                {
                    DWORD retCode =
                        HttpTerminate(HTTP_INITIALIZE_SERVER | HTTP_INITIALIZE_CONFIG, NULL);
                    BOOST_ASSERT(retCode == NO_ERROR);
                }
            };

        } // namespace http
    } // namespace winasio
} // namespace boost

#endif // BOOST_WINASIO_HTTP_INITIALIZER_HPP
