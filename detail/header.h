/*********************************************************
          File Name: sha1.h
          Author: Abby Cin
          Mail: abbytsing@gmail.com
          Created Time: Sat 29 Jun 2019 11:13:36 AM CST
**********************************************************/

#ifndef HEADER_H_
#define HEADER_H_

namespace http
{
  constexpr char CRLF[] = "\r\n";
  constexpr char CRLFs[] = "\r\n\r\n";
  constexpr char GUID[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

  class header
  {
  public:
    static header_error parse_accept_header(header& h, char* data, size_t n)
    {
      static nm::string_view prefix{"HTTP/1.1"};
      nm::string_view v{data, n};
      if(v.size() >= prefix.size() && !v.starts_with(prefix))
      {
        return header_error::invalid_status_line;
      }

      auto end = v.find(CRLFs);
      if(end == v.npos)
      {
        return header_error::more;
      }
      h.set_size(end + 4); // including CRLFs

      auto index = v.find_first_of(CRLF);
      auto line = v.substr(0, index).trim();

      // parse status line
      {
        line = line.substr(prefix.size(), line.size()).trim();
        auto idx = line.find(' ');
        if(idx == line.npos)
        {
          return header_error::invalid_status_line;
        }

        auto code = line.substr(0, idx).trim();

        h.set_code(code);
        if(code != "101")
        {
          return header_error::invalid_status_line;
        }
      }

      auto old = index;
      while(index < end)
      {
        old = index + 2;
        index = v.find_first_of(CRLF, old);
        line = v.substr(old, index - old);
        auto sep = line.find(':');
        if(sep == line.npos)
        {
          return header_error::invalid_field_format;
        }
        to_lower(data + old, sep);
        auto key = line.substr(0, sep).trim();
        auto val = line.substr(sep + 1, index - sep).trim();
        h.header_.emplace(key, val);
      }

      if(h.field("upgrade") != "websocket")
      {
        return header_error::invalid_upgrade;
      }

      auto connection = h.field("connection").to_string();
      std::transform(connection.begin(), connection.end(), connection.begin(), [](auto c) { return std::tolower(c); });
      if(connection.find("upgrade") == connection.npos)
      {
        return header_error::invalid_connection;
      }

      auto accept_key = h.field("sec-websocket-accept");
      if(accept_key.empty())
      {
        return header_error::invalid_sec_ws_acc;
      }

      auto key = h.field("sec-websocket-key");
      if(!ws_verify_key(key, accept_key))
      {
        return header_error::invalid_sec_ws_key;
      }
      // TODO: handle optional Sec-Websocket-Extensions, Sec-Websocket-Protocol
      return header_error::ok;
    }

    static header_error parse_upgrade_header(header& h, char* data, size_t n)
    {
      static nm::string_view prefix{"GET"};
      nm::string_view v{data, n};
      if(v.size() >= prefix.size() && !v.starts_with(prefix))
      {
        return header_error::invalid_status_line;
      }
      auto end = v.find(CRLFs);
      if(end == v.npos)
      {
        return header_error::more;
      }
      h.set_size(end + 4); // including CRLFs

      auto index = v.find_first_of(CRLF);
      auto line = v.substr(0, index).trim();

      // parse status line
      {
        auto idx = line.rfind("HTTP/");
        if(idx == line.npos)
        {
          return header_error::invalid_status_line;
        }

        auto ver = line.substr(idx + 5, line.size()).trim();
        if(ver != "1.1")
        {
          return header_error::invalid_status_line;
        }

        auto path = line.substr(0, idx).remove_prefix(prefix.size()).trim();
        h.set_version(ver);
        h.set_path(path);
      }

      auto old = index;
      while(index < end)
      {
        old = index + 2;
        index = v.find_first_of(CRLF, old);
        line = v.substr(old, index - old);
        auto sep = line.find(':');
        if(sep == line.npos)
        {
          return header_error::invalid_field_format;
        }
        to_lower(data + old, sep);
        auto key = line.substr(0, sep).trim();
        auto val = line.substr(sep + 1, index - sep).trim();
        h.header_.emplace(key, val);
      }

      if(h.field("host").empty())
      {
        return header_error::invalid_host;
      }

      if(h.field("upgrade") != "websocket")
      {
        return header_error::invalid_upgrade;
      }

      auto connection = h.field("connection").to_string();
      std::transform(connection.begin(), connection.end(), connection.begin(), [](auto c) { return std::tolower(c); });
      if(connection.find("upgrade") == connection.npos)
      {
        return header_error::invalid_connection;
      }

      if(h.field("sec-websocket-key").empty())
      {
        return header_error::invalid_sec_ws_key;
      }

      if(h.field("sec-websocket-version") != "13")
      {
        return header_error::invalid_ws_version;
      }

      return header_error::ok;
    }

    static header build_upgrade_header(const nm::string_view& host, const nm::string_view& target)
    {
      nm::string_view v = host;
      header h{};
      h.set_role(true);
      h.set_path(target);
      if(v.ends_with("/"))
      {
        v.remove_suffix(1);
      }
      if(host.starts_with("wss://"))
      {
        v.remove_prefix(6);
      }
      else if(v.starts_with("ws://"))
      {
        v.remove_prefix(5);
      }
      else
      {
        throw std::runtime_error("invalid host, must be ws:// or wss://");
      }
      h.set("host", v);
      h.set("upgrade", "websocket");
      h.set("connection", "Upgrade");
      h.set("sec-websocket-key", ws_gen_key().data());
      h.set("sec-websocket-version", "13");
      return h;
    }

    static std::string build_accept_header(header& src)
    {
      auto key = src.field("sec-websocket-key");
      header h{};
      h.set_role(false);
      h.set_version("1.1");
      h.set_code("101");
      h.set("upgrade", "websocket");
      h.set("connection", "Upgrade");
      h.set("sec-websocket-accept", ws_gen_accept_key(key).data());

      return h.build();
    }

    header() = default;

    header(header&& h) noexcept
        : is_client_{h.is_client_}, method_{h.method_}, code_{h.code_}, path_{h.path_}, version_{h.version_},
          header_{std::move(h.header_)}
    {
    }

    header& operator=(header&& h) noexcept
    {
      if(this != &h)
      {
        is_client_ = h.is_client_;
        method_ = h.method_;
        code_ = h.code_;
        path_ = h.path_;
        version_ = h.version_;
        header_ = std::move(h.header_);
      }
      return *this;
    }

    std::string build() const
    {
      std::string res{};
      if(is_client_)
      {
        res = "GET " + path_.to_string() + " HTTP/1.1" + CRLF; // compatible with http2 too
      }
      else
      {
        res.append("HTTP/1.1 101 Switching Protocol");
        res.append(CRLF);
      }
      for(auto& iter: header_)
      {
        res.append(iter.first.to_string());
        res.append(":");
        res.append(iter.second.to_string());
        res.append(CRLF);
      }
      res.append(CRLF);
      return res;
    }

    size_t size() { return size_; }

    void set_role(bool is_client) { is_client_ = is_client; }

    void set_version(const nm::string_view& v) { version_ = v; }

    void set(const nm::string_view& key, const nm::string_view& value) { header_.emplace(key, value); }

    void set_path(const nm::string_view& path) { path_ = path; }

    void set_code(const nm::string_view& code) { code_ = code; }

    const nm::string_view& field(const nm::string_view& key)
    {
      static nm::string_view empty;
      auto iter = header_.find(key);
      if(iter == header_.end())
      {
        return empty;
      }
      return iter->second;
    }

    nm::string_view& path() { return path_; }

  private:
    bool is_client_{false};
    size_t size_{};
    nm::string_view method_{};
    nm::string_view code_{};
    nm::string_view path_{};
    nm::string_view version_{};
    using header_t = std::map<nm::string_view, nm::string_view>;
    header_t header_{};

    void set_size(size_t n) { size_ = n; }

    static void to_lower(char* data, size_t n)
    {
      for(size_t i = 0; i < n; ++i)
      {
        data[i] = std::tolower(data[i]);
      }
    }

    static const std::string& ws_gen_key()
    {
      constexpr static char b[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
      constexpr static int len = 16;
      static std::random_device rd;
      static thread_local std::string key;
      std::mt19937 eng{rd()};
      std::string res(len, '\0');
      for(int i = 0; i < len; ++i)
      {
        res[i] = b[eng() % len];
      }
      key = nm::base64_encode(res);
      return key;
    }

    static const std::string& ws_gen_accept_key(const nm::string_view& key)
    {
      static thread_local std::string acc_key;
      std::string tmp = key.to_string() + GUID;
      tmp = nm::Sha1::sha1(tmp);
      acc_key = nm::base64_encode(tmp);
      return acc_key;
    }

    static bool ws_verify_key(const nm::string_view& key, const nm::string_view& accept_key)
    {
      nm::string_view ekey = ws_gen_accept_key(key);
      if(accept_key == ekey)
      {
        return true;
      }
      return false;
    }
  };
}

#endif
