#include "EventLoopThread.h"
#include "EventLoop.h"

EventLoopThread::EventLoopThread(const ThreadInitCallback &cb,
const std::string &name)
    : loop_(nullptr)
    , exiting_(false)
    , thread_(std::bind(&EventLoopThread::threadFunc, this), name)
    , mutex_()
    , cond_()
    , callback_(cb)
{
}
EventLoopThread::~EventLoopThread(){
    exiting_ = true;
    if(loop_ != nullptr ){
        // 仅仅调用 quit() 并不会立即结束事件循环线程。
        // 它只会设置一个标志，表示事件循环应该退出
        loop_->quit();
        // join() 方法阻塞当前线程（即析构函数所在的线程）
        // 直到 thread_ 对应的事件循环线程执行完毕
        thread_.join();
    }
}

EventLoop* EventLoopThread::startLoop(){
    // 执行thread里绑定的func
    thread_.start();
    EventLoop *loop = nullptr;

    // 下面复杂的逻辑只是为了确保事件循环线程成功创建
    {
        std::unique_lock<std::mutex> lock(mutex_);
        // 使用带谓词的wait，当loop_不为nullptr时返回
        cond_.wait(lock, [this]{return loop_ != nullptr;});
        loop = loop_;
    }
    // 防御性编程
    // 在临界区内获取共享状态的快照，然后在临界区外使用这个快照
    return loop;
}


void EventLoopThread::threadFunc(){
    EventLoop loop;
    // 可能存在的初始化
    if(callback_){
        callback_(&loop);
    }
{
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ =  &loop;
    // 通知 startLoop() 的 while(loop_ == nullptr)
    cond_.notify_one();
}
    loop.loop();
    // 再次获取锁
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr;
}