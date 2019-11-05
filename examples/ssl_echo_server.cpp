/*********************************************************
          File Name: ssl_echo_server.cpp
          Author: Abby Cin
          Mail: abbytsing@gmail.com
          Created Time: Sun 14 Jul 2019 09:10:33 AM CST
**********************************************************/

#include "common.h"
#include "asio/ssl.hpp"
#include "websocket.h"
#include <iostream>
#include <set>

using sock_t = ws::stream<asio::ssl::stream<asio::ip::tcp::socket>>;

class EchoHanlder : public std::enable_shared_from_this<EchoHanlder>
{
public:
  explicit EchoHanlder(asio::io_context& ioc, asio::ssl::context& ctx) : socket_{ioc, ctx} {}

  static std::shared_ptr<EchoHanlder> create_instance(asio::io_context& ioc, asio::ssl::context& ctx)
  {
    return std::make_shared<EchoHanlder>(ioc, ctx);
  }

  asio::ip::tcp::socket& socket() { return socket_.next_layer().next_layer(); }

  void run()
  {
    auto self = shared_from_this();
    socket_.set_ping_msg("are you ok?", std::chrono::seconds(5));
    std::error_code ec{};
    socket_.next_layer().handshake(asio::ssl::stream_base::server, ec);
    if(ec)
    {
      std::cerr << "ssl handshake: " << ec.message() << '\n';
    }
    else
    {
      socket_.accept([self, this](http::header& h, const std::error_code& e) {
        if(e)
        {
          std::cerr << "websocket accept: " << e.message() << '\n';
        }
        else
        {
          do_read();
        }
      });
    }
  }

private:
  sock_t socket_;

  void do_read()
  {
    auto self = shared_from_this();
    socket_.read([self, this](const std::error_code& ec, const ws::Buffer& b) {
      if(ec)
      {
        std::cerr << "read: " << ec.message() << ':' << b << '\n';
        socket_.force_close();
      }
      else
      {
        do_write(b);
      }
    });
  }

  void do_write(const ws::Buffer& b)
  {
    auto self = shared_from_this();
    socket_.write_text(b, [self, this](const std::error_code& ec, size_t) {
      if(ec)
      {
        std::cerr << "write: " << ec.message() << '\n';
        socket_.force_close();
      }
      else
      {
        do_read();
      }
    });
  }
};

template<typename Handler = EchoHanlder>
class WssServer
{
public:
  WssServer(asio::ip::tcp::endpoint ep, int n)
      : pool_{n}, ctx_{asio::ssl::context::tlsv12_server}, acceptor_{}, signal_{}
  {
    signal_.reset(new asio::signal_set{pool_.get_context()});
    signal_->add(SIGINT);
    signal_->add(SIGTERM);
    signal_->async_wait([this](const asio::error_code& e, int s) {
      if(e)
      {
        std::cerr << e.message() << '\n';
      }
      else
      {
        std::cerr << "received signal " << s << ", exit...\n";
      }
      this->stop();
    });

    acceptor_.reset(new asio::ip::tcp::acceptor{pool_.get_context()});
    acceptor_->open(ep.protocol());
    acceptor_->set_option(asio::socket_base::reuse_address(true));
#ifdef __linux__
    using reuse_port = asio::detail::socket_option::boolean<SOL_SOCKET, SO_REUSEPORT>;
    acceptor_->set_option(reuse_port(true));
#endif
    acceptor_->bind(ep);
    acceptor_->listen();
  }

  ~WssServer() { stop(); }

  void run(const nm::string_view& cert, const nm::string_view& key)
  {
    ctx_.set_options(ctx_.tls_server | ctx_.no_tlsv1 | ctx_.no_tlsv1_1);
    // verify client, for high security requirement
    // ctx_.set_verify_mode(ctx_.verify_peer | ctx_.verify_fail_if_no_peer_cert);
    ctx_.set_password_callback([](auto, auto) { return "no password"; });
    ctx_.use_certificate_chain_file(cert.data());
    ctx_.use_private_key_file(key.data(), ctx_.pem);

    do_accept();

    pool_.run();
  }

  void do_accept()
  {
    auto conn = Handler::create_instance(pool_.get_context(), ctx_);
    acceptor_->async_accept(conn->socket(), [conn, this](const std::error_code& ec) {
      if(!ec)
      {
        conn->run();
        do_accept();
      }
    });
  }

  void stop() { pool_.stop(); }

private:
  IoContextPool pool_;
  asio::ssl::context ctx_;
  std::unique_ptr<asio::ip::tcp::acceptor> acceptor_;
  std::unique_ptr<asio::signal_set> signal_{};
};

int main(int argc, char* argv[])
{
  int n = 1;
  if(argc == 2)
  {
    n = std::stoi(argv[1]);
  }
  asio::io_context ioc{};
  WssServer<EchoHanlder> server{asio::ip::tcp::endpoint{asio::ip::address_v4::any(), 8889}, n};
  server.run("cert.pem", "key.pem");
}
