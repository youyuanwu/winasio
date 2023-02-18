#define BOOST_TEST_MODULE named_pipe
#include <boost/test/unit_test.hpp>

#include "named_pipe/echoserver_movable.hpp"
#include "test_client.hpp"

#include "named_pipe/echoserver.hpp"

#include <semaphore>

template <typename Server> void test_server() {
  auto server_count = 2; // std::thread::hardware_concurrency() * 2;
  net::io_context io_context(server_count);

  winnet::named_pipe_protocol<net::io_context::executor_type>::endpoint ep(
      "\\\\.\\pipe\\mynamedpipe");

  Server s(io_context, ep);

  std::vector<std::thread> server_threads;

  for (int n = 0; n < server_count; ++n) {
    server_threads.emplace_back([&] { io_context.run(); });
  }

  // let server warm up
  std::this_thread::sleep_for(std::chrono::seconds(1));

  // lauch client threads
  int const client_count = 3;

  std::counting_semaphore<client_count> sem{0};

  std::vector<std::thread> client_threads;
  for (int n = 0; n < client_count; ++n) {
    client_threads.emplace_back([n, &sem] {
      sem.acquire();
      std::string myMessage =
          std::string("myMessage") + std::to_string(n) + "hi";
      std::string replyMsg;
      boost::system::error_code ec = make_client_call(myMessage, replyMsg);
      BOOST_CHECK_MESSAGE(!ec.failed(), myMessage + " failed: " + ec.message());
      if (!ec.failed()) {
        BOOST_CHECK_EQUAL(myMessage, replyMsg);
      }
      sem.release();
    });
  }

  // try in this tread to make sure it is working
  std::string myMessage = std::string("myMessage adhoc");
  std::string replyMsg;
  boost::system::error_code ec = make_client_call(myMessage, replyMsg);
  BOOST_CHECK(!ec.failed());
  if (!ec.failed()) {
    BOOST_CHECK_EQUAL(myMessage, replyMsg);
  }

  // let 3 clients race at the same time.
  sem.release(client_count);

  for (auto &thread : client_threads) {
    if (thread.joinable()) {
      thread.join();
    }
  }

  // finish server
  io_context.stop();
  for (auto &thread : server_threads) {
    if (thread.joinable()) {
      thread.join();
    }
  }

  io_context.reset();
}

BOOST_AUTO_TEST_SUITE(test_named_pipe)

BOOST_AUTO_TEST_CASE(movable_server) { test_server<server_movable>(); }

BOOST_AUTO_TEST_CASE(nonmovable_server) { test_server<server>(); }

BOOST_AUTO_TEST_SUITE_END()
