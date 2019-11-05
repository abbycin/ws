/*********************************************************
          File Name: echo_client.cpp
          Author: Abby Cin
          Mail: abbytsing@gmail.com
          Created Time: Sat 13 Jul 2019 10:01:23 AM CST
**********************************************************/

#include "common.h"
#include "websocket.h"
#include <iostream>

using sock = ws::stream<asio::ip::tcp::socket>;

class Client
{
public:
  Client(asio::io_context& ioc, const asio::ip::tcp::endpoint& ep, int num) : sock_{ioc}, ep_{ep}, limit_{num}
  {
    sock_.is_mask_set();
    sock_.set_mask(true);
    sock_.set_fragment_size(4096);
    sock_.set_max_message_size(1 << 20);
    sock_.max_message_size();
    sock_.set_ping_msg("ping", std::chrono::seconds(5));
    sock_.next_layer().async_connect(ep_, [this](const std::error_code& ec) {
      if(ec)
      {
        std::cerr << "connect: " << ec.message() << '\n';
      }
      else
      {
        sock_.handshake("ws://127.0.0.1:8889", "/", [this](const std::error_code& ec) {
          if(ec)
          {
            std::cerr << "handshake: " << ec.message() << '\n';
            sock_.next_layer().get_io_context().stop();
          }
          else
          {
            this->do_write();
          }
        });
      }
    });
  }

private:
  sock sock_;
  asio::ip::tcp::endpoint ep_;
  int limit_;
  int count_ = 0;
  int send_bytes_ = 0;
  std::string ss{
      "++1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1ss+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+"
      "1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+"
      "1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+"
      "1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+"
      "1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+"
      "1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+"
      "1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+"
      "1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+"
      "1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+"
      "1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+"
      "1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+"
      "1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+"
      "1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+"
      "1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+"
      "1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+"
      "1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1ss+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+"
      "1s+1s+1s+1s+1s+1s+s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+s+"
      "1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+s+1s+1s+1s+1s+1s+1s+"
      "1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+"
      "1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+"
      "1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+"
      "1s+1s+1s+1s+1s+1s+s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+s+"
      "1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+s+1s+1s+1s+1s+1s+1s+"
      "1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+"
      "1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+"
      "1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+"
      "1s+1s+1s+1s+1s+1s+s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+s+"
      "1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+s+1s+1s+1s+1s+1s+1s+"
      "1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+"
      "1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+"
      "1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+"
      "1s+1s+1s+1s+1s+1s+s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+s+"
      "1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+s+1s+1s+1s+1s+1s+1s+"
      "1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+"
      "1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+"
      "1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+"
      "1s+1s+1s+1s+1s+1s+s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+s+"
      "1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+s+1s+1s+1s+1s+1s+1s+"
      "1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+"
      "1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+"
      "1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+"
      "1s+1s+1s+1s+1s+1s+s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+s+"
      "1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+s+1s+1s+1s+1s+1s+1s+"
      "1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+"
      "1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+"
      "1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+"
      "1s+1s+1s+1s+1s+1s+s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+s+"
      "1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s++1s+1s+1s+1s+1s+1s+1s+"
      "1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s+1s1s+1s"};

  void do_write()
  {
    sock_.write_text(ws::buffer(ss), [this](const std::error_code& ec, size_t n) {
      if(ec)
      {
        std::cerr << "write: " << ec.message() << '\n';
      }
      else
      {
        send_bytes_ = ss.size();
        do_read();
      }
    });
  }

  void do_read()
  {
    sock_.read([this](const std::error_code& ec, const ws::Buffer& s) {
      if(ec)
      {
        sock_.force_close();
        std::cerr << "read: " << ec.message() << ':' << s << '\n';
      }
      else
      {
        assert(s.readable_size() == send_bytes_);
        count_ += 1;
        if(count_ == limit_)
        {
          sock_.close();
        }
        else
        {
          do_write();
        }
        ws::is_utf8(s);
        // std::cerr << "count: " << count_ /*<< ", " << ws::is_utf8(s) */ << '\n';
      }
    });
  }
};

int main(int argc, char* argv[])
{
  int n = 1;
  if(argc == 2)
  {
    n = std::stoi(argv[1]);
  }

  int num = 10'000'000;
  std::cerr << "send && recv " << (n * num) << " messages\n";
  using namespace std::chrono;
  auto b = high_resolution_clock::now();

  ThreadGroup tg{};
  for(int i = 0; i < n; ++i)
  {
    tg.spawn([num] {
      asio::io_context ioc;
      Client client{ioc, asio::ip::tcp::endpoint{asio::ip::address_v4::loopback(), 8889}, num};
      ioc.run();
    });
  }
  tg.join();
  auto e = high_resolution_clock::now();
  auto elapsed = duration_cast<nanoseconds>(e - b).count() / 1'000'000'000.0;
  std::cerr << "elapsed: " << elapsed << "s\n";
}
