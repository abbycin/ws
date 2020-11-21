/*********************************************************
          File Name: stream_impl.h
          Author: Abby Cin
          Mail: abbytsing@gmail.com
          Created Time: Sat 1 Jun 2019 10:56:23 AM CST
**********************************************************/

#ifndef WS_STREAM_IMPL_H
#define WS_STREAM_IMPL_H

namespace ws
{
  inline bool is_utf8(const Buffer& b) { return nm::UTF8::validate(b.peek(), b.readable_size()); }

  namespace
  {
    template<typename T>
    struct SocketWrapper
    {
      SocketWrapper(T& t) : sock{t} {}

      void cancel(std::error_code& ec) { sock.next_layer().cancel(ec); }

      void close(std::error_code& ec) { sock.next_layer().close(ec); }

      // FIXME: remove cast
      asio::io_context& context() { return static_cast<asio::io_context&>(sock.next_layer().get_executor().context()); }

      void shutdown_wr()
      {
        std::error_code ec{};
        sock.next_layer().shutdown(asio::ip::tcp::socket::shutdown_send, ec);
      }

      T& sock;
    };
    template<>
    struct SocketWrapper<asio::ip::tcp::socket>
    {
      SocketWrapper(asio::ip::tcp::socket& t) : sock{t} {}

      void cancel(std::error_code& ec) { sock.cancel(ec); }

      void close(std::error_code& ec) { sock.close(ec); }

      // FIXME: remove cast
      asio::io_context& context() { return static_cast<asio::io_context&>(sock.get_executor().context()); }

      void shutdown_wr()
      {
        std::error_code ec{};
        sock.shutdown(asio::ip::tcp::socket::shutdown_send, ec);
      }

      asio::ip::tcp::socket& sock;
    };
  }

  template<typename NextLayer>
  class stream<NextLayer>::impl : public std::enable_shared_from_this<stream<NextLayer>::impl>
  {
  public:
    constexpr static size_t kFragmentSize = 4096;
    constexpr static size_t kMaxMsgSize = kFragmentSize * 4;
    constexpr static size_t kMinFrameSize = 2;
    constexpr static size_t kMaxFrameSize = 14;
    using std::enable_shared_from_this<stream<NextLayer>::impl>::shared_from_this;

    template<typename... Args>
    explicit impl(Args&&... args)
        : msg_type_{NONE}, status_{CLOSED}, fragment_size_{kFragmentSize}, max_msg_size_{kMaxMsgSize},
          socket_{std::forward<Args>(args)...}, wrapper_{socket_}, timer_{socket_.get_executor()},
          last_error_{}, ping_msg_{}, ping_interval_{},
          last_heartbeat_{}, frame_{}, header_{}, rd_buf_{kMaxMsgSize}, ctrl_{}, payload_{kMaxFrameSize}, wr_buf_{}
    {
    }

    template<typename... Args>
    static std::shared_ptr<impl> create(Args&&... args)
    {
      return std::make_shared<impl>(std::forward<Args>(args)...);
    }

    impl& operator=(impl&& rhs) = delete;

    impl(const impl&) = delete;

    impl& operator=(const impl&) = delete;

    ~impl() { this->force_close(); }

    void set_fragment_size(size_t n = kFragmentSize) { fragment_size_ = n < kFragmentSize ? kFragmentSize : n; }

    void set_max_message_size(size_t n = kMaxMsgSize)
    {
      max_msg_size_ = n < kMaxMsgSize ? kMaxMsgSize : n;
      payload_.make_space(max_msg_size_);
    }

    size_t fragment_size() { return fragment_size_; }

    size_t max_message_size() { return max_msg_size_; }

    void set_ping_msg(const std::string& msg, const std::chrono::seconds& interval)
    {
      ping_msg_ = msg;
      ping_interval_ = interval;
    }

    bool is_bin() { return msg_type_ == BINARY; }

    bool is_text() { return msg_type_ == TEXT; }

    void set_mask(bool off) { mask_ = off; }

    bool is_mask_set() { return mask_; }

    std::error_code last_error() { return last_error_; }

    bool is_open() { return status_ == OPENED; }

    NextLayer& next_layer() { return socket_; }

    void handshake(const nm::string_view& host, const nm::string_view& target, HandshakeCallback cb)
    {
      if(status_ != CLOSED)
      {
        cb(make_error_code(ws_error::already_opened));
        return;
      }
      else
      {
        start_timer();
        header_ = http::header::build_upgrade_header(host, target);
        auto tmp = header_.build();
        wr_buf_.append(tmp.data(), tmp.size());
        auto buf = buffer(wr_buf_.peek(), wr_buf_.readable_size());
        auto self = shared_from_this();
        this->write_impl(buf, [self, this, cb = std::move(cb)](const std::error_code& ec, size_t) {
          if(ec)
          {
            cb(ec);
            return;
          }
          rd_buf_.reset();
          wr_buf_.reset();
          this->handshake_impl(cb);
        });
      }
    }

    void accept(AcceptCallback cb)
    {
      if(status_ != CLOSED)
      {
        cb(header_, make_error_code(ws_error::already_opened));
      }
      else
      {
        start_timer();
        rd_buf_.reset();
        this->accept_impl(cb);
      }
    }

    void accept(const Buffer& data, AcceptCallback cb)
    {
      // assume data contains complete upgrade header
      rd_buf_.append(data.peek(), data.readable_size());
      auto r = http::header::parse_upgrade_header(header_, rd_buf_.peek(), rd_buf_.readable_size());
      if(http::header_error::ok != r)
      {
        cb(header_, http::make_error_code(r));
        return;
      }

      auto tmp = http::header::build_accept_header(header_);

      wr_buf_.append(tmp.data(), tmp.size());
      rd_buf_.read(header_.size());
      auto buf = buffer(wr_buf_.peek(), wr_buf_.readable_size());
      auto self = shared_from_this();
      this->write_impl(buf, [cb = std::move(cb), self, this](const std::error_code& ec, size_t) {
        if(!ec)
        {
          status_ = OPENED;
          this->start_timer();
        }
        wr_buf_.reset();
        cb(header_, ec);
      });
    }

    void close(close_code c = close_code::normal, const Buffer& msg = {})
    {
      if(status_ == OPENED)
      {
        status_ = CLOSING_1;
        write_close_msg(c, msg);
      }
      else
      {
        last_error_ = make_error_code(ws_error::already_closed);
      }
    }

    void read(RecvCallback cb)
    {
      if(status_ != CLOSED)
      {
        this->read_impl(cb);
      }
      else
      {
        cb(make_error_code(ws_error::already_closed), {});
      }
    }

    void write_text(const Buffer& buf, SendCallback cb)
    {
      if(status_ == OPENED)
      {
        this->prepare_write(buf, detail::opcode::text, cb);
      }
      else
      {
        cb(make_error_code(ws_error::already_closed), 0);
      }
    }

    void write_binary(const Buffer& buf, SendCallback cb)
    {
      if(status_ == OPENED)
      {
        this->prepare_write(buf, detail::opcode::binary, cb);
      }
      else
      {
        cb(make_error_code(ws_error::already_closed), 0);
      }
    }

    asio::io_context& context() { return wrapper_.context(); }

    void force_close()
    {
      if(status_ != CLOSED)
      {
        status_ = CLOSED;
        std::error_code ec;
        timer_.cancel(ec);
        wrapper_.cancel(ec);
        wrapper_.close(ec);
      }
    }

  private:
    enum MsgType
    {
      NONE = 0,
      TEXT,
      BINARY
    };

    enum ConnStatus
    {
      OPENED = 0,
      CLOSING_1 = 1,
      CLOSING_2 = 2,
      CLOSED = 3
    };

    bool mask_{false};
    bool sending_{false};
    MsgType msg_type_;
    ConnStatus status_;
    size_t fragment_size_;
    size_t max_msg_size_;
    NextLayer socket_;
    SocketWrapper<NextLayer> wrapper_;
    asio::steady_timer timer_;
    std::error_code last_error_;
    std::string ping_msg_;
    std::chrono::seconds ping_interval_;
    std::chrono::system_clock::time_point last_heartbeat_;
    http::header header_;
    detail::WsFrame frame_;
    detail::Message rd_buf_;
    detail::Message ctrl_;
    detail::Message payload_;
    detail::Message wr_buf_;

    void mask(char* data, size_t n, detail::WsFrame& f)
    {
      if(f.is_mask_set())
      {
        detail::mask(data, n, f.mask_key());
      }
    }

    void ping(const Buffer& payload = {}) { this->write_ping_pong(payload, detail::opcode::ping); }

    void pong(const Buffer& payload = {}) { this->write_ping_pong(payload, detail::opcode::pong); }

    void restart_timer()
    {
      if(ping_interval_.count() > 0)
      {
        timer_.expires_after(ping_interval_);
        auto self = shared_from_this();
        timer_.async_wait([self, this](const std::error_code& e) {
          if(e)
          {
            if(e != asio::error::operation_aborted)
            {
              force_close();
            }
          }
          else
          {
            using namespace std::chrono;
            auto delta = duration_cast<seconds>(system_clock::now() - last_heartbeat_);
            if(delta > ping_interval_)
            {
              if(status_ == CLOSED)
              {
                this->force_close(); // upgrade timeout
              }
              else
              {
                this->close(close_code::going_away, "peer going away");
              }
            }
            else
            {
              this->ping(buffer(ping_msg_));
              restart_timer();
            }
          }
        });
      }
    }

    void start_timer()
    {
      last_heartbeat_ = std::chrono::system_clock::now();
      this->restart_timer();
    }

    void update_heartbeat() { last_heartbeat_ = std::chrono::system_clock::now(); }

    void set_msg_type()
    {
      if(frame_.is_text())
      {
        msg_type_ = TEXT;
      }
      else if(frame_.is_binary())
      {
        msg_type_ = BINARY;
      }
      else
      {
        msg_type_ = NONE;
      }
    }

    void build_write_buffer(detail::Message& out, bool fin, detail::opcode code, size_t payload_size, const Buffer& buf)
    {
      detail::WsFrame f{};
      if(fin)
      {
        f.set_fin();
      }
      else
      {
        f.unset_fin();
      }
      f.set_mask(mask_);
      f.set_code(code);
      f.set_payload_size(payload_size);
      out.make_space(kMaxFrameSize);
      auto n = f.build(out.begin_write());
      out.write(n);
      out.append(buf.peek(), buf.readable_size());
      mask(out.begin_write() - buf.readable_size(), buf.readable_size(), f);
    }

    std::string handle_ctrl_msg(const Buffer& v)
    {
      if(frame_.is_ping())
      {
        this->pong(v);
      }
      else if(frame_.is_pong())
      {
        this->update_heartbeat();
      }
      else if(frame_.is_close())
      {
        close_code c = close_code::no_close_code; // no close message payload
        auto r = detail::decode_close_msg(v, c);
        if(status_ == OPENED)
        {
          status_ = CLOSING_2;
          this->write_close_msg(c, r);
        }
        else if(status_ == CLOSING_2)
        {
          this->force_close();
        }
        std::string s = "code: " + std::to_string(static_cast<int>(c)) + ", reason: ";
        s.append(r.peek(), r.readable_size());
        return s;
      }
      return {};
    }

    void write_close_msg(close_code c, const Buffer& msg)
    {
      size_t payload_size = msg.readable_size();
      if(payload_size > 0x7b)
      {
        payload_size = 0x7b;
      }
      char data[0x7d];
      ctrl_.reset();
      auto r = detail::build_close_msg(data, c, msg.peek(), payload_size);
      build_write_buffer(ctrl_, true, detail::opcode::close, r, {data, r});

      if(status_ == CLOSING_2)
      {
        std::error_code ec{};
        asio::write(socket_, asio::buffer(ctrl_.peek(), ctrl_.readable_size()), ec);
        force_close();
      }
      else
      {
        ctrl_.set_close();
        this->write_control_message();
      }
    }

    void write_ping_pong(const Buffer& payload, detail::opcode c)
    {
      if(status_ == OPENED)
      {
        auto payload_size = payload.readable_size() > 0x7d ? 0x7d : payload.readable_size();
        build_write_buffer(ctrl_, true, c, payload_size, {payload.peek(), payload_size});
        this->write_control_message();
      }
    }

    void write_control_message()
    {
      if(ctrl_.readable_size() == 0)
      {
        return;
      }
      if(!sending_)
      {
        sending_ = true;
        auto buf = buffer(ctrl_.peek(), ctrl_.readable_size());
        auto self = shared_from_this();
        this->write_impl(buf, [self, this](const std::error_code& ec, size_t nbytes) {
          sending_ = false;
          if(ec)
          {
            last_error_ = ec;
            return;
          }
          ctrl_.read(nbytes);
          if(ctrl_.is_close())
          {
            status_ = CLOSING_2;
            wrapper_.shutdown_wr();
            std::error_code e{};
            timer_.cancel(e);
            read_impl([self, this](const std::error_code& ec, const Buffer&) {
              if(ec)
              {
                last_error_ = ec;
              }
            });
            wr_buf_.reset();
          }
        });
      }
    }

    void prepare_write(const Buffer& payload, detail::opcode code, SendCallback cb)
    {
      if(payload.readable_size() > max_msg_size_)
      {
        cb(make_error_code(ws_error::payload_too_big), 0);
        return;
      }

      wr_buf_.set_bytes(payload.readable_size());
      while(payload.readable_size())
      {
        size_t payload_size = payload.readable_size();
        bool fin = true;
        if(payload_size > fragment_size_)
        {
          fin = false;
          payload_size = fragment_size_;
        }
        wr_buf_.make_space(fragment_size_ + kMaxFrameSize);
        build_write_buffer(wr_buf_, fin, code, payload_size, {payload.peek(), payload_size});
        payload.read(payload_size);
        code = detail::opcode::cont;
      }
      write_message(cb);
    }

    void write_message(SendCallback cb)
    {
      if(status_ != OPENED)
      {
        cb(make_error_code(ws_error::already_closed), 0);
        return;
      }

      if(last_error_)
      {
        cb(last_error_, 0);
        return;
      }

      if(sending_)
      {
        auto self = shared_from_this();
        wrapper_.context().post([self, this, cb = std::move(cb)] { write_message(cb); });
        return;
      }

      if(wr_buf_.readable_size() == 0)
      {
        return;
      }

      sending_ = true;
      auto buf = buffer(wr_buf_.peek(), wr_buf_.readable_size());
      auto self = shared_from_this();
      this->write_impl(buf, [self, this, cb = std::move(cb)](const std::error_code& ec, size_t) {
        sending_ = false;
        cb(ec, wr_buf_.bytes());
        wr_buf_.reset();
        this->write_control_message();
      });
    }

    void handshake_impl(HandshakeCallback cb)
    {
      auto buf = asio::buffer(rd_buf_.peek() + rd_buf_.readable_size(), rd_buf_.size() - rd_buf_.readable_size());
      auto self = shared_from_this();
      socket_.async_read_some(buf, [self, this, cb = std::move(cb)](const std::error_code& ec, size_t nbytes) {
        rd_buf_.write(nbytes);
        if(ec)
        {
          cb(ec);
        }
        else
        {
          using namespace http;
          auto r = header::parse_accept_header(header_, rd_buf_.peek(), rd_buf_.readable_size());
          if(http::header_error::more == r)
          {
            if(rd_buf_.writable_size() == 0)
            {
              cb(http::make_error_code(http::header_error::header_too_big));
            }
            else
            {
              this->handshake_impl(cb);
            }
          }
          else if(http::header_error::ok == r)
          {
            rd_buf_.read(header_.size());
            status_ = OPENED;
            wr_buf_.reset();
            cb(http::make_error_code(http::header_error::ok));
            this->start_timer();
          }
          else
          {
            cb(http::make_error_code(r));
          }
        }
      });
    }

    void accept_impl(AcceptCallback cb)
    {
      auto b = asio::buffer(rd_buf_.peek() + rd_buf_.readable_size(), rd_buf_.writable_size());
      auto self = shared_from_this();
      socket_.async_read_some(b, [cb = std::move(cb), self, this](const std::error_code& ec, size_t nbytes) {
        rd_buf_.write(nbytes);
        if(ec)
        {
          cb(header_, ec);
        }
        else
        {
          auto r = http::header::parse_upgrade_header(header_, rd_buf_.peek(), rd_buf_.readable_size());
          if(http::header_error::more == r)
          {
            if(rd_buf_.writable_size() == 0)
            {
              cb(header_, http::make_error_code(http::header_error::header_too_big));
            }
            else
            {
              this->accept_impl(cb);
            }
            return;
          }

          if(http::header_error::ok != r)
          {
            cb(header_, http::make_error_code(r));
            return;
          }

          auto tmp = http::header::build_accept_header(header_);
          wr_buf_.append(tmp.data(), tmp.size());
          rd_buf_.read(header_.size());
          auto buf = buffer(wr_buf_.peek(), wr_buf_.readable_size());
          this->write_impl(buf, [cb = std::move(cb), self, this](const std::error_code& ec, size_t) {
            if(!ec)
            {
              status_ = OPENED;
              wr_buf_.reset();
              this->start_timer();
            }
            cb(header_, ec);
          });
        }
      });
    }

    template<typename Callback>
    void write_impl(const Buffer& buf, Callback cb)
    {
      asio::async_write(socket_, asio::buffer(buf.peek(), buf.readable_size()),
                        [cb = std::move(cb)](const std::error_code& e, size_t nbytes) { cb(e, nbytes); });
    }

    void read_impl(RecvCallback cb)
    {
      for(; status_ == OPENED || status_ == CLOSING_2;)
      {
        if(last_error_)
        {
          cb(last_error_, {});
          return;
        }
        auto e = frame_.parse_frame(rd_buf_.peek(), rd_buf_.readable_size());
        if(e)
        {
          cb(e, {});
          return;
        }

        if(frame_.is_complete())
        {
          if(payload_.readable_size() + frame_.payload_size() > max_message_size())
          {
            cb(make_error_code(ws_error::payload_too_big), {});
            return;
          }
        }

        if(!frame_.is_complete() || rd_buf_.readable_size() < frame_.size())
        {
          rd_buf_.make_space(fragment_size_ + kMaxFrameSize);
          auto buf = asio::buffer(rd_buf_.begin_write(), rd_buf_.writable_size());
          auto self = shared_from_this();
          socket_.async_read_some(buf, [cb, self, this](const std::error_code& ec, size_t nbytes) {
            if(ec)
            {
              cb(ec, {});
            }
            else
            {
              rd_buf_.write(nbytes);
              this->read_impl(cb);
            }
          });
          return;
        }

        // don't wait for close frame
        if(status_ == CLOSING_2 && !frame_.is_close())
        {
          cb(make_error_code(ws_error::expect_close), {});
          return;
        }

        rd_buf_.read(frame_.frame_size());

        this->mask(rd_buf_.peek(), frame_.payload_size(), frame_);
        auto b = buffer(rd_buf_.peek(), frame_.payload_size());
        rd_buf_.read(frame_.payload_size());
        if(frame_.is_control())
        {
          auto r = handle_ctrl_msg(b);
          if(!frame_.is_close())
          {
            continue;
          }
          else
          {
            cb(make_error_code(ws_error::closed), buffer(r));
            return;
          }
        }
        else
        {
          payload_.append(b.peek(), b.readable_size());
          if(frame_.is_fin())
          {
            set_msg_type();
            b = buffer(payload_.peek(), payload_.readable_size());
            cb({}, b);
            payload_.reset();
            return;
          }
        }
        frame_.clear();
      }
    }
  };

  template<typename NextLayer>
  template<typename... Args>
  stream<NextLayer>::stream(Args&&... args) : layer_{}
  {
    layer_ = impl::create(std::forward<Args>(args)...);
  }

  template<typename NextLayer>
  stream<NextLayer>::stream(stream&& rhs) noexcept : layer_{std::move(rhs.layer_)}
  {
  }

  template<typename NextLayer>
  stream<NextLayer>& stream<NextLayer>::operator=(stream&& rhs) noexcept
  {
    if(this != &rhs)
    {
      this->~stream();
      new(this) stream<NextLayer>{std::move(rhs)};
    }
    return *this;
  }

  template<typename NextLayer>
  void stream<NextLayer>::set_ping_msg(const std::string& msg, const std::chrono::seconds& interval)
  {
    layer_->set_ping_msg(msg, interval);
  }

  template<typename NextLayer>
  bool stream<NextLayer>::is_bin()
  {
    return layer_->is_bin();
  }

  template<typename NextLayer>
  bool stream<NextLayer>::is_text()
  {
    return layer_->is_text();
  }

  template<typename NextLayer>
  bool stream<NextLayer>::is_open()
  {
    return layer_->is_open();
  }

  template<typename NextLayer>
  void stream<NextLayer>::set_fragment_size(size_t n)
  {
    layer_->set_fragment_size(n);
  }

  template<typename NextLayer>
  void stream<NextLayer>::set_max_message_size(size_t n)
  {
    layer_->set_max_message_size(n);
  }

  template<typename NextLayer>
  size_t stream<NextLayer>::fragment_size()
  {
    return layer_->fragment_size();
  }

  template<typename NextLayer>
  size_t stream<NextLayer>::max_message_size()
  {
    return layer_->max_message_size();
  }

  template<typename NextLayer>
  void stream<NextLayer>::set_mask(bool on)
  {
    layer_->set_mask(on);
  }

  template<typename NextLayer>
  bool stream<NextLayer>::is_mask_set()
  {
    return layer_->is_mask_set();
  }

  template<typename NextLayer>
  std::error_code stream<NextLayer>::last_error()
  {
    return layer_->last_error();
  }

  template<typename NextLayer>
  NextLayer& stream<NextLayer>::next_layer()
  {
    return layer_->next_layer();
  }

  template<typename NextLayer>
  typename NextLayer::lowest_layer_type& stream<NextLayer>::lowest_layer()
  {
    return layer_->next_layer().lowest_layer();
  }

  template<typename NextLayer>
  void stream<NextLayer>::handshake(const nm::string_view& h, const nm::string_view& p, HandshakeCallback cb)
  {
    layer_->handshake(h, p, cb);
  }

  template<typename NextLayer>
  void stream<NextLayer>::accept(AcceptCallback cb)
  {
    layer_->accept(cb);
  }

  template<typename NextLayer>
  void stream<NextLayer>::accept(const Buffer& data, AcceptCallback cb)
  {
    layer_->accept(data, cb);
  }

  template<typename NextLayer>
  void stream<NextLayer>::close(close_code c, const Buffer& msg)
  {
    layer_->close(c, msg);
  }

  template<typename NextLayer>
  void stream<NextLayer>::force_close()
  {
    layer_->force_close();
  }

  template<typename NextLayer>
  void stream<NextLayer>::read(RecvCallback cb)
  {
    layer_->read(cb);
  }

  template<typename NextLayer>
  void stream<NextLayer>::write_text(const Buffer& buf, SendCallback cb)
  {
    layer_->write_text(buf, cb);
  }

  template<typename NextLayer>
  void stream<NextLayer>::write_binary(const Buffer& buf, SendCallback cb)
  {
    layer_->write_binary(buf, cb);
  }

  template<typename NextLayer>
  asio::io_context& stream<NextLayer>::context()
  {
    return layer_->context();
  }
}

#endif // WS_STREAM_IMPL_H
