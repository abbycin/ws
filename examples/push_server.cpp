/***********************************************
        File Name: push_server.cpp
        Author: Abby Cin
        Mail: abbytsing@gmail.com
        Created Time: 4/24/20 10:04 PM
***********************************************/

#include "common.h"
#include "websocket.h"
#include <iostream>
#include <set>

using sock_t = ws::stream<asio::ip::tcp::socket>;

class PushHandler;
using PushHandlerPtr = std::shared_ptr<PushHandler>;
using PushHandlerWPtr = std::weak_ptr<PushHandler>;

class WsServer
{
public:
  WsServer(asio::ip::tcp::endpoint ep, int n);

  ~WsServer()
  {
    if(is_running_)
    {
      stop();
    }
  }

  void run();

  void accept();

  void stop();

  void insert(PushHandlerPtr conn);

  void push(const ws::Buffer& buf);

private:
  bool is_running_{false};
  std::mutex mtx_{};
  IoContextPool pool_;
  std::unique_ptr<asio::ip::tcp::acceptor> acceptor_;
  std::unique_ptr<asio::signal_set> signals_;
  std::list<PushHandlerWPtr> clients_;
};

class PushHandler : public std::enable_shared_from_this<PushHandler>
{
public:
  explicit PushHandler(asio::io_context& ioc, WsServer* server) : sock_{ioc}, server_{server} {}

  ~PushHandler()
  {
    std::cerr << __func__ << '\n';
  }

  static std::shared_ptr<PushHandler> create_instance(asio::io_context& ioc, WsServer* server)
  {
    return std::make_shared<PushHandler>(ioc, server);
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

  bool alive() { return alive_; }

  void send(const ws::Buffer& buf)
  {
    auto self = shared_from_this();
    sock_.write_text(buf, [self, this](const std::error_code& ec, size_t) {
      if(ec)
      {
        std::cerr << "send error: " << ec.message() << '\n';
        alive_ = false;
        sock_.force_close();
      }
    });
  }

private:
  bool alive_{true};
  sock_t sock_;
  WsServer* server_;

  void do_read()
  {
    if(alive_)
    {
      auto self = shared_from_this();
      sock_.read([self, this](const std::error_code& ec, const ws::Buffer& buf) {
        if(ec)
        {
          std::cerr << "read: " << ec.message() << "; " << buf << '\n';
          sock_.force_close();
        }
        else
        {
          server_->push(buf);
          do_read();
        }
      });
    }
  }
};

WsServer::WsServer(asio::ip::tcp::endpoint ep, int n) : pool_{n}, acceptor_{}, signals_{}
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

void WsServer::run()
{
  accept();
  is_running_ = true;
  pool_.run();
}

void WsServer::accept()
{
  acceptor_->async_accept([this](const std::error_code& e, asio::ip::tcp::socket sock) {
    if(e)
    {
      std::cerr << e.message() << '\n';
      return;
    }
    auto conn = PushHandler::create_instance(pool_.get_context(), this);
    conn->socket() = std::move(sock);
    insert(conn);
    conn->run();
    accept();
  });
}

void WsServer::push(const ws::Buffer& buf)
{
  std::lock_guard<std::mutex> lg{mtx_};
  // `buf` will be invalid after `read`, since it's a view of internal read buffer
  std::string data{buf.peek(), buf.readable_size()};
  for(auto iter = clients_.begin(); iter != clients_.end();)
  {
    auto c = iter->lock();
    if(c)
    {
      if(c->alive())
      {
        c->send(ws::buffer(data));
      }
      ++iter;
    }
    else
    {
      iter = clients_.erase(iter);
    }
  }
}

void WsServer::insert(PushHandlerPtr conn)
{
  std::lock_guard<std::mutex> lg{mtx_};
  clients_.push_back(conn);
}

void WsServer::stop()
{
  if(is_running_)
  {
    std::error_code ec;
    acceptor_->cancel(ec);
    pool_.stop();
    is_running_ = false;
    clients_.clear();
  }
}

int main(int argc, char* argv[])
{
  int n = 1;
  if(argc == 2)
  {
    n = std::stoi(argv[1]);
  }
  WsServer server{asio::ip::tcp::endpoint{asio::ip::address_v4::any(), 8889}, n};
  server.run();
}