#include "EventLoop.h"
#include "Logger.h"
#include "Poller.h"
#include "Channel.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/eventfd.h>
#include <memory>
//忘了errno
#include <errno.h>

// 防止一个线程创建多个EventLoop
thread_local EventLoop *t_loopInThisThread = nullptr;

// 定义默认的POLLER IO复用接口的超时时间
const int kPollTimeMs = 10000;


// 创建wakeupfd,用来notify唤醒subReactor处理新来的channel
int createEventfd(){
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0){
        LOG_FATAL("eventfd error:%d\n", errno);
    }
    return evtfd;
}

EventLoop::EventLoop()
    : looping_(false)
    , quit_(false)
    , callingPendingFunctors_(false)
    , threadId_(CurrentThread::tid())
    , poller_(Poller::newDefaultPoller(this))
    , wakeupFd_(createEventfd())
    , wakeupChannel_(new Channel(this, wakeupFd_))
    , currentActiveChannel_(nullptr){
        LOG_DEBUG("EventLoop created %p in thread %d \n", this, threadId_);
        if(t_loopInThisThread){
            LOG_FATAL("Another EventLoop %p exists in this thread %d \n", t_loopInThisThread, threadId_);
        }else{
            t_loopInThisThread = this;
        }

        // 设置wakeupfd的事件类型和以及发生事件后的回调操作
        // wakeupChannel_是独占此channel的智能指针
        wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
        wakeupChannel_->enableReading();
}

EventLoop::~EventLoop(){
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    // Close the file descriptor FD.
    close(wakeupFd_);
    // 忘了
    t_loopInThisThread = nullptr;

}
// 开启事件循环
void EventLoop::loop(){
    looping_ = true;
    quit_ = false;

    LOG_INFO("EventLoop %p start looping \n", this);

    while( !quit_ ){
        activeChannels_.clear();
        // 监听两类fd, client 的 fd 和 wakeupfd --> mainloop 唤醒 subloop 用,
        // 调用 poller_ 的 poll 方法进行事件轮询
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        for (Channel *channel : activeChannels_){
            // 
            channel->handleEvevnt(pollReturnTime_);
        }
        // 执行当前EventLoop事件循环需要处理的回调操作
        // mainLoop事先注册一个回调cb,wakeup subloop后,执行下面的方法(是mainLoop注册的cb)
        doPendingFunctors();
    }
    LOG_INFO("EventLoop %p stop looping \n", this);
    looping_ = false;
}
// 退出事件循环
// 1. loop在自己的线程里调用quit()
void EventLoop::quit(){
    quit_ = true;
    // 跨线程退出: 如果调用 quit() 函数的线程不是EventLoop所在的线程，就需要调用 wakeup() 函数。
    // EventLoop可能正在 poller_->poll() 阻塞 等待事件发生。
    // 在这种情况下，即使设置了 quit_ 为 true，事件循环也不会立即感知到这个变化，仍然会阻塞在 poll 调用上。
    if(!isInLoopThread()){
        wakeup();
    }
}

// 确保传入的任务 cb 在 EventLoop 所在的线程中执行
void EventLoop::runInLoop(Functor cb){
    if (isInLoopThread()){ 
        // 在当前loop线程中,执行cb
        cb();
    }else{
        // 在非当前loop线程中执行cb, 需要唤醒loop所在线程,执行cb
        queueInLoop(cb);
    }
}

// 把cb放入事件循环的待执行函数队列中，等待事件循环在下一次迭代时执行
void EventLoop::queueInLoop(Functor cb){
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }
    // callingPendingFunctors_ = true 表示有新的回调要执行
    // 如果不加这个判断,事件循环在doPendingFunctors()之后又卡在poll上
    if(!isInLoopThread() || callingPendingFunctors_){
        wakeup();
    }
}



void EventLoop::handleRead(){
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_, &one, sizeof one);
    if (n != sizeof one){
        LOG_ERROR("EventLoop::handleRead() reads %ld bytes instead of 8", n);
    }
}

// 通过向wakeupFd_写入数据，触发wakeupChannel_的可读事件，从而唤醒正在阻塞的事件循环
void EventLoop::wakeup(){
    uint64_t one = 1;
    ssize_t n = write(wakeupFd_, &one, sizeof one);
    if(n != sizeof one){
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes istead of 8 \n", n);
    }
}

// EventLoop的方法->Poller的方法
void EventLoop::updateChannel(Channel* channel){
    LOG_INFO("EventLoop-updateChannel!!!!!");
    poller_->updateChannel(channel);
}
void EventLoop::removeChannel(Channel* channel){
    poller_->removeChannel(channel);
}
bool EventLoop::hasChannel(Channel* channel){
    return poller_->hasChannel(channel);
}

void EventLoop::doPendingFunctors(){
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for(const Functor &functor : functors){
        // 执行当前事件循环需要执行的回调操作
        functor();
    }
    callingPendingFunctors_ = false;
}