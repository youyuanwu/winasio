#include "named_pipe/echoserver_movable.hpp"
#include "test_client.hpp"
#include "gtest/gtest.h"

#include "named_pipe/echoserver.hpp"

#include <semaphore>

template <typename Server>
void test_server()
{
    auto server_count = 2; // std::thread::hardware_concurrency() * 2;
    net::io_context io_context(server_count);

    winnet::named_pipe_protocol<net::io_context::executor_type>::endpoint ep(
        "\\\\.\\pipe\\mynamedpipe");

    Server s(io_context, ep);

    std::vector<std::thread> server_threads;

    for (int n = 0; n < server_count; ++n)
    {
        server_threads.emplace_back([&]
                                    { io_context.run(); });
    }

    // let server warm up
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // lauch client threads
    int const client_count = 3;

    std::counting_semaphore<client_count> sem{0};

    std::vector<std::thread> client_threads;
    for (int n = 0; n < client_count; ++n)
    {
        client_threads.emplace_back([n, &sem]
                                    {
      sem.acquire();
      std::string myMessage =
          std::string("myMessage") + std::to_string(n) + "hi";
      std::string replyMsg;
      boost::system::error_code ec = make_client_call(myMessage, replyMsg);
      ASSERT_FALSE(ec.failed()) << myMessage << " failed: " << ec.message();
      if (!ec.failed()) {
        ASSERT_EQ(myMessage, replyMsg);
        // std::cout << (std::string("!!!!!!!!") +  myMessage + " success") <<
        // std::endl;
      }
      sem.release(); });
    }

    // try in this tread to make sure it is working
    std::string myMessage = std::string("myMessage adhoc");
    std::string replyMsg;
    boost::system::error_code ec = make_client_call(myMessage, replyMsg);
    ASSERT_FALSE(ec.failed());
    if (!ec.failed())
    {
        ASSERT_EQ(myMessage, replyMsg);
    }

    // let 3 clients race at the same time.
    sem.release(client_count);

    for (auto & thread : client_threads)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }

    // finish server
    io_context.stop();
    for (auto & thread : server_threads)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }

    io_context.reset();
}

TEST(NamedPipe, movable_server)
{
    test_server<server_movable>();
}

TEST(NamedPipe, server)
{
    test_server<server>();
}
