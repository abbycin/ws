/*********************************************************
          File Name: buffer.h
          Author: Abby Cin
          Mail: abbytsing@gmail.com
          Created Time: Sat 29 Jun 2019 09:17:32 AM CST
**********************************************************/

#ifndef WS_BUFFER_H
#define WS_BUFFER_H

namespace ws
{
  namespace detail
  {
    class Message
    {
    public:
      constexpr static size_t kDefaultSize = 1024;

      explicit Message(size_t n = kDefaultSize) : close_{false}, bytes_{0}, read_idx_{0}, write_idx_{0}, data_{}
      {
        if(n == 0)
        {
          n = kDefaultSize;
        }
        data_.resize(n);
      }

      Message(Message&& rhs) noexcept
          : close_{rhs.close_}, bytes_{rhs.bytes_}, read_idx_{rhs.read_idx_},
            write_idx_{rhs.write_idx_}, data_{std::move(rhs.data_)}
      {
        rhs.reset();
      }

      Message& operator=(Message&& rhs) noexcept
      {
        if(this != &rhs)
        {
          data_ = std::move(rhs.data_);
          close_ = rhs.close_;
          bytes_ = rhs.bytes_;
          read_idx_ = rhs.read_idx_;
          write_idx_ = rhs.write_idx_;
        }
        return *this;
      }

      // call when finish reading/writing
      void reset()
      {
        bytes_ = 0;
        read_idx_ = 0;
        write_idx_ = 0;
      }

      void rewind()
      {
        if(read_idx_ != 0)
        {
          std::copy(peek(), peek() + readable_size(), data_.begin());
          write_idx_ = readable_size();
          read_idx_ = 0;
        }
      }

      void shrink_to(size_t n = kDefaultSize)
      {
        data_.resize(n);
        data_.shrink_to_fit();
        reset();
      }

      bool make_space(size_t n)
      {
        if(writable_size() >= n)
        {
          return false;
        }
        if(consumed() + writable_size() >= n)
        {
          std::copy(data_.begin() + read_idx_, data_.begin() + write_idx_, data_.begin());
          write_idx_ = readable_size();
          read_idx_ = 0;
          return false;
        }
        if(size() * 2 > readable_size() + n)
        {
          n = size() * 2;
        }
        else
        {
          n += readable_size();
        }
        Message tmp{n};
        tmp.append(peek(), readable_size());
        swap(tmp);
        return true;
      }

      size_t readable_size() const
      {
        assert(write_idx_ >= read_idx_);
        return write_idx_ - read_idx_;
      }

      size_t writable_size() const
      {
        assert(data_.size() >= write_idx_);
        return data_.size() - write_idx_;
      }

      size_t size() const { return data_.size(); }

      char* peek() { return data_.data() + read_idx_; }

      char* begin_write() { return data_.data() + write_idx_; }

      const char* peek() const { return data_.data() + read_idx_; }

      void append(const char* data, size_t n)
      {
        make_space(n);
        std::copy(data, data + n, begin_write());
        write_idx_ += n;
      }

      void read(size_t n) { read_idx_ += n; }

      size_t consumed() const { return read_idx_; }

      void write(size_t n) { write_idx_ += n; }

      void set_bytes(size_t n) { bytes_ = n; }

      size_t bytes() const { return bytes_; }

      void set_close() { close_ = true; }

      bool is_close() const { return close_; }

    public:
      bool close_;
      size_t bytes_;
      size_t read_idx_;
      size_t write_idx_;
      std::vector<char> data_;

      void swap(Message& rhs)
      {
        data_.swap(rhs.data_);
        read_idx_ = rhs.read_idx_;
        write_idx_ = rhs.write_idx_;
      }
    };
  }

  class Buffer
  {
  public:
    constexpr Buffer() : data_{nullptr}, read_idx_{0}, write_idx_{0} {}

    constexpr Buffer(const char* data, size_t n) : data_{data}, read_idx_{0}, write_idx_{n} {}

    explicit Buffer(const char* data) : data_{data}, read_idx_{0}, write_idx_{::strlen(data)} {}

    template<size_t N>
    Buffer(const char (&a)[N]) : data_{a}, read_idx_{0}, write_idx_{N - 1}
    {
    }

    Buffer(const Buffer& rhs) : Buffer{}
    {
      data_ = rhs.data_;
      read_idx_ = rhs.read_idx_;
      write_idx_ = rhs.write_idx_;
    }

    Buffer& operator=(const Buffer& rhs)
    {
      if(this != &rhs)
      {
        data_ = rhs.data_;
        read_idx_ = rhs.read_idx_;
        write_idx_ = rhs.write_idx_;
      }
      return *this;
    }

    const char* peek() const { return data_ + read_idx_; }

    size_t readable_size() const { return write_idx_ - read_idx_; }

    size_t consumed() const { return read_idx_; }

    void read(size_t n) const { read_idx_ += n; }

  private:
    const char* data_;
    mutable size_t read_idx_;
    mutable size_t write_idx_;
  };

  inline std::ostream& operator<<(std::ostream& os, const Buffer& b)
  {
    os.write(b.peek(), b.readable_size());
    return os;
  }

  constexpr Buffer buffer(const char* data, size_t n) { return {data, n}; }

  template<typename T>
  Buffer buffer(const T& x)
  {
    return {x.data(), x.size()};
  }

  inline Buffer buffer(const char* data) { return {data, ::strlen(data)}; }

  template<size_t N>
  constexpr Buffer buffer(const char (&a)[N])
  {
    return {a, N - 1};
  }
}

#endif // WS_BUFFER_H
