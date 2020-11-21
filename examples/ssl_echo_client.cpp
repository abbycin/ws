/*********************************************************
          File Name: ssl_echo_client.cpp
          Author: Abby Cin
          Mail: abbytsing@gmail.com
          Created Time: Sun 14 Jul 2019 03:05:41 PM CST
**********************************************************/

#include "common.h"
#include "asio/ssl.hpp"
#include "websocket.h"
#include <iostream>

using sock = ws::stream<asio::ssl::stream<asio::ip::tcp::socket>>;

class Client
{
public:
  Client(asio::io_context& ioc, int num) : ctx_{asio::ssl::context::tlsv12_client}, sock_{ioc, ctx_}, limit_{num}
  {
    sock_.is_mask_set();
    sock_.set_mask(false);
    sock_.set_fragment_size(4096);
    sock_.set_max_message_size(1 << 20);
  }

  ~Client()
  {
    if(sock_.is_open())
    {
      sock_.close();
    }
  }

  void run(const nm::string_view& host, const nm::string_view& path, const nm::string_view& cert)
  {
    connect_next_layer(host, cert);
    sock_.handshake(host, path, [this](const std::error_code& ec) {
      if(ec)
      {
        std::cerr << "websocket connect: " << ec.message() << '\n';
        sock_.context().stop();
      }
      else
      {
        this->do_write();
      }
    });
    sock_.context().run();
  }

private:
  asio::ssl::context ctx_;
  sock sock_;
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
    sock_.write_text(ws::buffer(ss), [this](const std::error_code& ec, size_t) {
      if(ec)
      {
        std::cerr << "write: " << ec.message() << '\n';
        sock_.force_close();
      }
      else
      {
        do_read();
      }
    });
    send_bytes_ = ss.size();
  }

  void do_read()
  {
    sock_.read([this](const std::error_code& ec, const ws::Buffer& b) {
      if(ec)
      {
        std::cerr << "read: " << ec.message() << ':' << b << '\n';
        sock_.force_close();
      }
      else
      {
        assert(b.readable_size() == send_bytes_);
        count_ += 1;
        if(count_ == limit_)
        {
          sock_.close();
        }
        else
        {
          do_write();
        }
        // std::cerr << "count: " << count_ /*<< ", " << ws::is_utf8(s) */ << '\n';
      }
    });
  }

  void connect_next_layer(nm::string_view host, nm::string_view cert)
  {
    using namespace asio::ip;
    asio::ip::tcp::endpoint ep;
    auto& next = sock_.next_layer();
    if(host.starts_with("wss://"))
    {
      host = host.substr(6);
      auto idx = host.find(':');
      auto h = host.substr(0, idx);
      auto p = host.substr(idx + 1);
      ep = tcp::endpoint(address::from_string(h.to_string()), std::stoi(p.data()));
      ctx_.set_verify_mode(ctx_.verify_peer);
      ctx_.load_verify_file(cert.data());
      ctx_.set_verify_callback([this](bool pre_verified, asio::ssl::verify_context& ctx) {
        char subject_name[256];
        X509* cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());
        X509_NAME_oneline(X509_get_subject_name(cert), subject_name, 256);
        std::cout << "Verifying " << subject_name << "\n";
        return pre_verified;
      });
    }
    else
    {
      throw std::runtime_error("invalid host");
    }
    asio::error_code ec;
    sock_.next_layer().lowest_layer().connect(ep, ec);
    if(ec)
    {
      throw std::runtime_error(ec.message());
    }
    sock_.next_layer().handshake(asio::ssl::stream_base::client, ec);
    if(ec)
    {
      throw std::runtime_error(ec.message());
    }
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
      Client client{ioc, num};
      client.run("wss://127.0.0.1:8889", "/", "cert.pem");
    });
  }
  tg.join();
  auto e = high_resolution_clock::now();
  auto elapsed = duration_cast<nanoseconds>(e - b).count() / 1'000'000'000.0;
  std::cerr << "elapsed: " << elapsed << "s\n";
}
