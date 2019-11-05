/*********************************************************
          File Name: frame.h
          Author: Abby Cin
          Mail: abbytsing@gmail.com
          Created Time: Sat 29 Jun 2019 09:18:10 AM CST
**********************************************************/

#ifndef FRAME_H_
#define FRAME_H_

#include "error.h"

namespace ws
{
  namespace detail
  {
    // network byte order to host byte order and viceversa
    // we can't using alias in C++11
    template<typename T>
    T translate(T value)
    {
      uint16_t x = 1;
      if(*(uint8_t*)&x == 0)
      {
        return value; // big endian
      }

      T res;
      char* p = (char*)&value - 1;
      char* e = p + sizeof(T);
      char* r = (char*)&res + sizeof(T);
      while(p != e)
      {
        *--r = *++p;
      }
      return res;
    }

    /*
       0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-------+-+-------------+-------------------------------+
   |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
   |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
   |N|V|V|V|       |S|             |   (if payload len==126/127)   |
   | |1|2|3|       |K|             |                               |
   +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
   |     Extended payload length continued, if payload len == 127  |
   + - - - - - - - - - - - - - - - +-------------------------------+
   |                               |Masking-key, if MASK set to 1  |
   +-------------------------------+-------------------------------+
   | Masking-key (continued)       |          Payload Data         |
   +-------------------------------- - - - - - - - - - - - - - - - +
   :                     Payload Data continued ...                :
   + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
   |                     Payload Data continued ...                |
   +---------------------------------------------------------------+
     */
    enum class opcode : uint8_t
    {
      cont = 0,
      text = 1,
      binary = 2,
      rsv3 = 3,
      rsv4 = 4,
      rsv5 = 5,
      rsv6 = 6,
      rsv7 = 7,
      close = 8,
      ping = 9,
      pong = 10,
      crsvb = 11,
      crsvc = 12,
      crsvd = 13,
      crsve = 14,
      crsvf = 15
    };

    enum class close_code : uint16_t
    {
      no_close_code = 0,
      normal = 1000,
      going_away = 1001,
      protocol_error = 1002,
      unknown_data = 1003,
      abnormal_closure = 1006,
      bad_payload = 1007,
      policy_error = 1008,
      too_big = 1009,
      needs_extension = 1010,
      internal_error = 1011,
      service_restart = 1012,
      try_again_later = 1013,
    };

    // mask/unmask are the same
    inline void mask(char* data, size_t n, uint32_t mask_key)
    {
      constexpr auto width = sizeof(uint32_t);
      auto q = n / width;
      auto p1 = (uint32_t*)data;
      for(size_t i = 0; i < q; ++i)
      {
        p1[i] ^= mask_key;
      }
      auto key = (uint8_t*)&mask_key;
      auto p2 = (uint8_t*)data;
      for(size_t i = q * width; i < n; ++i)
      {
        p2[i] ^= key[i & 3u];
      }
    }

    inline size_t build_close_msg(char* payload, close_code c, const char* data, size_t n)
    {
      if(c != close_code::no_close_code)
      {
        *(uint16_t*)payload = translate<uint16_t>(static_cast<uint16_t>(c));
        std::memcpy(payload + sizeof(uint16_t), data, n);
        return sizeof(uint16_t) + n;
      }
      return 0;
    }

    inline ws::Buffer decode_close_msg(const ws::Buffer& b, close_code& c)
    {
      if(b.readable_size() > 0)
      {
        uint16_t i = *(uint16_t*)b.peek();
        i = translate<uint16_t>(i);
        c = static_cast<close_code>(i);
        return {b.peek() + sizeof(c), b.readable_size() - sizeof(c)};
      }
      return {};
    }

    class WsFrame
    {
    public:
      WsFrame() : fin_{0x0u}, opcode_{opcode::close}, mask_key_{0}, frame_len_{0}, payload_len_{0} {}

      std::error_code parse_frame(char* data, size_t n)
      {
        complete_ = false;
        // at least 2 bytes
        if(n < 2)
        {
          return {};
        }

        auto* rd = (uint8_t*)data;
        uint8_t* end = rd + n;

        // get fin, rsv1-3 and opcode
        fin_ = *rd & 0xf0u;
        if(fin_ != 0x80u && fin_ != 0x0u)
        {
          return make_error_code(ws_error::bad_frame);
        }

        switch(static_cast<uint8_t>(*rd & 0xfu))
        {
        case 0:
          opcode_ = opcode::cont;
          break;
        case 1:
          opcode_ = opcode::text;
          break;
        case 2:
          opcode_ = opcode::binary;
          break;
        case 8:
          opcode_ = opcode::close;
          break;
        case 9:
          opcode_ = opcode::ping;
          break;
        case 10:
          opcode_ = opcode::pong;
          break;
        default:
          return make_error_code(ws_error::unsupport_opcode);
        }

        if(!is_fin() && is_control())
        {
          return make_error_code(ws_error::bad_control_frame);
        }

        rd += 1;
        if(rd >= end)
        {
          return {};
        }

        mask_ = false;
        if(*rd & 0x80u)
        {
          mask_ = true;
        }
        unsigned int len = *rd & ~0x80u;
        rd += 1;

        if(len < 0x7e)
        {
          payload_len_ = len;
        }
        else if(len == 0x7e)
        {
          if(rd + 2 > end)
          {
            return {};
          }

          uint16_t x = *(uint16_t*)rd;
          // real payload length
          payload_len_ = translate<uint16_t>(x);
          rd += 2;
        }
        else if(len == 0x7fu)
        {
          if(rd + 8 > end)
          {
            return {};
          }
          uint64_t x = *(uint64_t*)rd;
          payload_len_ = translate<uint64_t>(x);
          rd += 8;
        }
        else
        {
          return make_error_code(ws_error::invalid_frame_length);
        }

        if(is_control() && payload_size() > 0x7d)
        {
          return make_error_code(ws_error::control_message_payload_too_big);
        }

        if(mask_)
        {
          // get mask key
          if(rd + 4 > end)
          {
            return {};
          }
          else
          {
            mask_key_ = *(uint32_t*)rd;
            rd += 4;
          }
        }
        frame_len_ = static_cast<uint32_t>(rd - (uint8_t*)data);
        complete_ = true;
        return {};
      }

      size_t build(char* data)
      {
        auto* p = (uint8_t*)data;
        *p = fin_ | static_cast<uint8_t>(opcode_);

        p += 1;

        // 1 byte
        if(payload_len_ < 0x7eu)
        {
          *p = 0x80u | static_cast<uint8_t>(payload_len_);
          p += 1;
        }
        // 2 bytes
        else if(payload_len_ >= 0x7e && payload_len_ < 0xffffu)
        {
          *p = 0x80u | 0x7eu;
          p += 1;
          *(uint16_t*)p = translate<uint16_t>(static_cast<uint16_t>(payload_len_));
          p += sizeof(uint16_t);
        }
        // 8 bytes
        else if(payload_len_ >= 0xffffu)
        {
          *p = 0x80u | 0x7fu;
          p += 1;
          auto x = translate<uint64_t>(payload_len_);
          *(uint64_t*)p = x;
          p += sizeof(uint64_t);
        }

        if(mask_)
        {
          *(uint32_t*)p = mask_key_;
          p += sizeof(uint32_t);
        }
        else
        {
          *(uint8_t*)(data + 1) &= ~0x80u;
        }

        frame_len_ = static_cast<uint32_t>((char*)p - data);
        return static_cast<size_t>(frame_len_);
      }

      void set_fin() { fin_ = 0x80u; }

      void unset_fin() { fin_ = 0x0u; }

      void set_mask(bool off)
      {
        mask_ = off;
        if(mask_)
        {
          mask_key_ = gen_mask_key();
        }
      }

      bool is_mask_set() { return mask_; }

      uint32_t mask_key() { return mask_key_; }

      void set_code(opcode c) { opcode_ = c; }

      opcode code() { return opcode_; }

      size_t frame_size() { return frame_len_; }

      void set_payload_size(size_t n) { payload_len_ = n; }

      uint64_t payload_size() { return payload_len_; }

      uint64_t size() { return payload_len_ + frame_len_; }

      bool is_control()
      {
        auto c = static_cast<uint8_t>(opcode_);
        return (c >= static_cast<uint8_t>(opcode::close) && c <= static_cast<uint8_t>(opcode::crsvf));
      }

      bool is_fin() { return fin_ == 0x80u; }

      bool is_ping() { return opcode_ == opcode::ping; }

      bool is_close() { return opcode_ == opcode::close; }

      bool is_pong() { return opcode_ == opcode::pong; }

      bool is_text() { return opcode_ == opcode::text; }

      bool is_binary() { return opcode_ == opcode::binary; }

      bool is_complete() { return complete_; }

      void clear()
      {
        fin_ = 0x0u;
        opcode_ = opcode::cont;
        mask_key_ = 0;
        frame_len_ = 0;
        payload_len_ = 0;
        complete_ = false;
      }

    private:
      bool mask_{false};
      bool complete_{false};
      uint8_t fin_;
      opcode opcode_;
      uint32_t mask_key_;
      uint32_t frame_len_;
      uint64_t payload_len_;

      uint32_t gen_mask_key()
      {
#if 0
        static std::random_device rd;
        std::mt19937 eng{rd()};
        return eng();
#else
        static unsigned long x = 123456789, y = 362436069, z = 521288629;

        unsigned long t;
        x ^= x << 16u;
        x ^= x >> 5u;
        x ^= x << 1u;

        t = x;
        x = y;
        y = z;
        z = t ^ x ^ y;

        return z;
#endif
      }
    };
  }
  using close_code = detail::close_code;
}

#endif // FRAME_H_
