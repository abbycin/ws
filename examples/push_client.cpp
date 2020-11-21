/***********************************************
        File Name: push_client.cpp
        Author: Abby Cin
        Mail: abbytsing@gmail.com
        Created Time: 4/25/20 12:13 PM
***********************************************/

#include "asio.hpp"
#include "websocket.h"
#include <iostream>

using sock = ws::stream<asio::ip::tcp::socket>;

class Client
{
public:
  Client(asio::io_context& ioc, const asio::ip::tcp::endpoint& ep, int num)
      : ioc_{ioc}, timer_{ioc}, sock_{ioc}, ep_{ep}, limit_{num}
  {
    ss_ = std::to_string(getpid());
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
        sock_.handshake("ws://127.0.0.1:8889", "/ws", [this](const std::error_code& ec) {
          if(ec)
          {
            std::cerr << "handshake: " << ec.message() << '\n';
            sock_.context().stop();
          }
          else
          {
            this->restart_timer();
            do_read();
          }
        });
      }
    });
  }

private:
  asio::io_context& ioc_;
  asio::steady_timer timer_;
  sock sock_;
  asio::ip::tcp::endpoint ep_;
  int limit_;
  int count_ = 0;
  std::string ss_{};

  void restart_timer()
  {
    timer_.expires_after(std::chrono::seconds(1));
    timer_.async_wait([this](const std::error_code& ec) {
      if(!ec)
      {
        this->do_write();
        count_ += 1;
        if(count_ < limit_)
        {
          restart_timer();
        }
      }
    });
  }

  void do_write()
  {
    sock_.write_text(ws::buffer(ss_), [this](const std::error_code& ec, size_t n) {
      if(ec)
      {
        std::cerr << "write: " << ec.message() << '\n';
        ioc_.stop();
      }
      else
      {
        std::cerr << "send: " << ss_ << '\n';
      }
    });
  }

  void do_read()
  {
    sock_.read([this](const std::error_code& ec, const ws::Buffer& s) {
      if(ec)
      {
        sock_.force_close();
        ioc_.stop();
        std::cerr << "read: " << ec.message() << '\n';
      }
      else
      {
        if(count_ >= limit_)
        {
          sock_.close();
        }
        else
        {
          do_read();
        }
        std::cerr << getpid() << " => " << s << '\n';
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

  int num = 10;
  std::cerr << "send && recv " << (n * num) << " messages\n";
  using namespace std::chrono;
  auto b = high_resolution_clock::now();

  asio::io_context ioc;
  Client client{ioc, asio::ip::tcp::endpoint{asio::ip::address_v4::loopback(), 8889}, num};
  ioc.run();

  auto e = high_resolution_clock::now();
  auto elapsed = duration_cast<nanoseconds>(e - b).count() / 1'000'000'000.0;
  std::cerr << "elapsed: " << elapsed << "s\n";
}
