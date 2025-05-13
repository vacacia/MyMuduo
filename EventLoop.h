#pragma once

#include "CurrentThread.h"
#include "Timestamp.h"
#include "noncopyable.h"
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

class Channel;
class Poller;

// include channel poller(epoll的抽象)
class EventLoop {
public:
  using Functor = std::function<void()>;

  EventLoop();
  ~EventLoop();

  // 开启事件循环
  void loop();
  // 退出事件循环
  void quit();

  Timestamp pollReturnTime() const { return pollReturnTime_; }

  // 在当前loop中执行cb
  void runInLoop(Functor cb);
  // 把cb放入队列中，唤醒loop所在的线程，执行cb
  void queueInLoop(Functor cb);

  // 唤醒loop所在线程
  void wakeup();

  // EventLoop的方法->Poller的方法
  void updateChannel(Channel *channel);
  void removeChannel(Channel *channel);
  bool hasChannel(Channel *channel);

  // 判断当前调用该方法的线程是否为 EventLoop 对象所在的线程。
  bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }

private:
  void handleRead();        // wake up
  void doPendingFunctors(); // 执行回调

  using ChannelList = std::vector<Channel *>;

  std::atomic_bool looping_; // 原子操作，通过cas实现
  std::atomic_bool quit_;    // 标识退出loop循环

  const pid_t threadId_;     // 记录当前loop所在线程的id
  Timestamp pollReturnTime_; // poller返回发生事件的channels的时间点
  std::unique_ptr<Poller> poller_;

  int wakeupFd_; // 当mainLoop获取一个新用户的channel，通过轮询算法选择一个subloop,通过该成员唤醒subloop处理channel
  std::unique_ptr<Channel> wakeupChannel_;

  ChannelList activeChannels_;
  Channel *currentActiveChannel_;

  std::atomic_bool
      callingPendingFunctors_; // 标识当前loop是否有需要执行的回调操作
  std::vector<Functor> pendingFunctors_; // 存储loop需要执行的所以回调操作
  std::mutex mutex_; // 互斥锁，保护上面vector容器的线程安全操作
};