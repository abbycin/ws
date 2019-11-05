/*********************************************************
          File Name: common.h
          Author: Abby Cin
          Mail: abbytsing@gmail.com
          Created Time: Fri 06 Mar 2020 08:22:37 PM CST
**********************************************************/
#ifndef WS_COMMON_H
#define WS_COMMON_H

#include <thread>
#include <vector>
#include "asio.hpp"

class ThreadGroup
{
public:
  ThreadGroup() : thread_{} {}

  ~ThreadGroup() { join(); }

  template<typename F, typename... Args>
  void spawn(F&& f, Args&&... args)
  {
    thread_.emplace_back(std::forward<F>(f), std::forward<Args>(args)...);
  }

  void join()
  {
    for(auto& t: thread_)
    {
      try
      {
        t.join();
      }
      catch(...)
      {
      }
    }
  }

private:
  std::vector<std::thread> thread_;
};

class IoContextPool
{
public:
  IoContextPool(int n = std::thread::hardware_concurrency())
  {
    for(int i = 0; i < n; ++i)
    {
      auto ctx = new asio::io_context{};
      work_.emplace_back(new asio::io_context::work{*ctx});
      ctx_.emplace_back(ctx);
    }
  }

  ~IoContextPool() { stop(); }

  asio::io_context& get_context()
  {
    if(cur_ == ctx_.size())
    {
      cur_ = 0;
    }
    return *ctx_[cur_++];
  }

  void run()
  {
    for(size_t i = 0; i < ctx_.size(); ++i)
    {
      worker_.emplace_back(new std::thread{[](std::shared_ptr<asio::io_context> ioc) { ioc->run(); }, ctx_[i]});
    }
    for(auto& t: worker_)
    {
      try
      {
        t->join();
      }
      catch(...)
      {
      }
    }
  }

  void stop()
  {
    for(auto& c: ctx_)
    {
      c->stop();
    }
  }

private:
  size_t cur_{0};
  std::vector<std::shared_ptr<asio::io_context>> ctx_{};
  std::vector<std::shared_ptr<asio::io_context::work>> work_{}; // work_ should destroy before ctx_
  std::vector<std::shared_ptr<std::thread>> worker_{};
};
#endif // WS_COMMON_H
