/*********************************************************
          File Name: error.h
          Author: Abby Cin
          Mail: abbytsing@gmail.com
          Created Time: Sat 29 Jun 2019 09:15:56 AM CST
**********************************************************/

#ifndef ERROR_CODE_H_
#define ERROR_CODE_H_

namespace ws
{
  enum class ws_error : int
  {
    ok,
    already_closed,
    closed,
    bad_frame,
    invalid_frame_length,
    payload_too_big,
    expect_close,
    already_opened,
    unsupport_opcode,
    bad_control_frame,
    control_message_payload_too_big
  };

  class ws_category_impl : public std::error_category
  {
  public:
    const char* name() const noexcept override { return "websocket"; }

    std::string message(int ev) const noexcept override
    {
      switch(static_cast<ws_error>(ev))
      {
      case ws_error::ok:
        break;
      case ws_error::already_closed:
        return "connection is already closed";
      case ws_error::bad_frame:
        return "frame can't be parsed";
      case ws_error::closed:
        return "close frame received";
      case ws_error::invalid_frame_length:
        return "invalid frame length";
      case ws_error::payload_too_big:
        return "payload to big";
      case ws_error::expect_close:
        return "expect close frame";
      case ws_error::already_opened:
        return "handshake already completed";
      case ws_error::unsupport_opcode:
        return "unsupport message received";
      case ws_error::bad_control_frame:
        return "bad control frame, control frame MUST NOT be fragmented";
      case ws_error::control_message_payload_too_big:
        return "control message payload too big";
      }

      return "ok";
    }

    // optional
    // omit overlapped error
    std::error_condition default_error_condition(int ev) const noexcept override
    {
      return std::error_condition(ev, *this);
    }
  };

  inline const std::error_category& ws_category()
  {
    static ws_category_impl instance{};
    return instance;
  }

  inline std::error_code make_error_code(ws_error e) { return std::error_code(static_cast<int>(e), ws_category()); }
}

namespace http
{
  enum class header_error : int
  {
    ok,
    more,
    header_too_big,
    invalid_status_line,
    invalid_field_format,
    invalid_upgrade,
    invalid_connection,
    invalid_sec_ws_acc,
    invalid_sec_ws_key,
    invalid_host,
    invalid_ws_version
  };

  class header_error_category : public std::error_category
  {
  public:
    const char* name() const noexcept override { return "http_header"; }

    std::string message(int ev) const noexcept override
    {
      switch(static_cast<header_error>(ev))
      {
      case header_error::ok:
        break;
      case header_error::more:
        return "need more";
      case header_error::header_too_big:
        return "handshake header too big";
      case header_error::invalid_status_line:
        return "invalid status line, expect GET /path HTTP/1.1 or HTTP/1.1 101 Switching Protocol";
      case header_error::invalid_field_format:
        return "invalid header field format";
      case header_error::invalid_upgrade:
        return "invalid upgrade field, expect upgrade: websocket";
      case header_error::invalid_connection:
        return "invalid connection field, expect connection: upgrade";
      case header_error::invalid_sec_ws_acc:
        return "sec-websocket-accept must be base64 encoded string";
      case header_error::invalid_sec_ws_key:
        return "either sec-websocket-key is invalid or verify failed";
      case header_error::invalid_host:
        return "invalid host";
      case header_error::invalid_ws_version:
        return "sec-websocket-version must be 13";
      }
      return "ok";
    }

    std::error_condition default_error_condition(int ev) const noexcept override
    {
      return std::error_condition(ev, *this);
    }
  };

  inline const std::error_category& ws_category()
  {
    static header_error_category instance{};
    return instance;
  }

  inline std::error_code make_error_code(header_error e) { return std::error_code(static_cast<int>(e), ws_category()); }
}

namespace std
{
  // mark ws_error convertible to std::error_code (equivalent)
  template<>
  struct is_error_code_enum<ws::ws_error> : true_type
  {
  };

  template<>
  struct is_error_code_enum<http::header_error> : true_type
  {
  };
}

#endif
