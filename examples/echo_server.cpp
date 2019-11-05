/*********************************************************
          File Name: echo_server.cpp
          Author: Abby Cin
          Mail: abbytsing@gmail.com
          Created Time: Sat 13 Jul 2019 08:19:56 AM CST
**********************************************************/

#include "common.h"
#include "websocket.h"
#include <iostream>
#include <set>

using sock_t = ws::stream<asio::ip::tcp::socket>;

class EchoHandler : public std::enable_shared_from_this<EchoHandler>
{
public:
  explicit EchoHandler(asio::io_context& ioc) : sock_{ioc} {}

  static std::shared_ptr<EchoHandler> create_instance(asio::io_context& ioc)
  {
    return std::make_shared<EchoHandler>(ioc);
  }

  asio::ip::tcp::socket& socket() { return sock_.next_layer(); }

  void run()
  {
    sock_.set_ping_msg("are you ok?", std::chrono::seconds(5));
    auto self = shared_from_this();
    sock_.accept([self, this](http::header& h, const std::error_code& ec) {
      if(ec)
      {
        std::cerr << "upgrade: " << ec.message() << '\n';
      }
      else
      {
        do_read();
      }
    });
  }

private:
  sock_t sock_;

  void do_read()
  {
    auto self = shared_from_this();
    sock_.read([self, this](const std::error_code& ec, const ws::Buffer& buf) {
      if(ec)
      {
        std::cerr << "read: " << ec.message() << ':' << buf << '\n';
        sock_.force_close();
      }
      else
      {
        do_write(buf);
      }
    });
  }

  void do_write(const ws::Buffer& buf)
  {
    auto self = shared_from_this();
    sock_.write_text(buf, [self, this](const std::error_code& ec, size_t nbytes) {
      if(ec)
      {
        sock_.force_close();
      }
      else
      {
        do_read();
      }
    });
  }
};

template<typename Handler>
class WsServer
{
public:
  WsServer(asio::ip::tcp::endpoint ep, int n) : pool_{n}, acceptor_{}, signals_{}
  {
    signals_.reset(new asio::signal_set{pool_.get_context()});
    signals_->add(SIGINT);
    signals_->add(SIGTERM);
    signals_->async_wait([this](const asio::error_code& e, int s) {
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
  };

  ~WsServer() { stop(); }

  void run()
  {
    accept();
    pool_.run();
  }

  void accept()
  {
    auto conn = Handler::create_instance(pool_.get_context());
    acceptor_->async_accept(conn->socket(), [conn, this](const std::error_code& e) {
      if(e)
      {
        std::cerr << e.message() << '\n';
        return;
      }
      conn->run();
      accept();
    });
  }

  void stop() { pool_.stop(); }

private:
  IoContextPool pool_;
  std::unique_ptr<asio::ip::tcp::acceptor> acceptor_;
  std::unique_ptr<asio::signal_set> signals_;
};

int main(int argc, char* argv[])
{
  int n = 1;
  if(argc == 2)
  {
    n = std::stoi(argv[1]);
  }
  WsServer<EchoHandler> server{asio::ip::tcp::endpoint{asio::ip::address_v4::any(), 8889}, n};
  server.run();
}
