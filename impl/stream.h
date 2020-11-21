/*********************************************************
          File Name: stream.h
          Author: Abby Cin
          Mail: abbytsing@gmail.com
          Created Time: Sat 1 Jun 2019 10:31:09 AM CST
**********************************************************/

#ifndef WS_STREAM_H
#define WS_STREAM_H

namespace ws
{
  using AcceptCallback = std::function<void(http::header&, const std::error_code&)>;
  using HandshakeCallback = std::function<void(const std::error_code&)>;
  using RecvCallback = std::function<void(const std::error_code&, const Buffer&)>;
  using SendCallback = std::function<void(const std::error_code&, size_t)>;

  template<typename NextLayer>
  class stream
  {
  public:
    template<typename... Args>
    explicit stream(Args&&... args);

    stream(stream&& rhs) noexcept;

    stream(const stream&) = delete;

    stream& operator=(stream&& rhs) noexcept;

    stream& operator=(const stream&) = delete;

    ~stream() = default;

    void set_ping_msg(const std::string& msg, const std::chrono::seconds& interval);

    bool is_bin();

    bool is_text();

    bool is_open();

    void set_fragment_size(size_t n);

    void set_max_message_size(size_t n);

    size_t fragment_size();

    size_t max_message_size();

    void set_mask(bool off);

    bool is_mask_set();

    std::error_code last_error();

    NextLayer& next_layer();

    typename NextLayer::lowest_layer_type& lowest_layer();

    void handshake(const nm::string_view& host, const nm::string_view& target, HandshakeCallback cb);

    void accept(AcceptCallback cb);

    void accept(const Buffer& data, AcceptCallback cb);

    void close(close_code c = close_code::normal, const Buffer& msg = {});

    void force_close();

    void read(RecvCallback cb);

    void write_text(const Buffer& buf, SendCallback cb);

    void write_binary(const Buffer& buf, SendCallback cb);

    asio::io_context& context();

  private:
    class impl;
    std::shared_ptr<impl> layer_;
  };
}

#endif // WS_STREAM_H