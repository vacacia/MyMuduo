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
        while(loop_ == nullptr){
            // wait 会阻塞主线程，并自动释放mutex锁
            cond_.wait(lock);
        }
        // 只要唤醒了主线程，就会取得锁，loop_就不会被修改，这步赋值不会出错
        loop = loop_;
    }
    return loop;
}

// 在单独的新线程运行
// 事件循环线程的实际工作是在 threadFunc() 函数中完成
void EventLoopThread::threadFunc(){
    // 创建独立的EventLoop，和线程一一对应
    EventLoop loop;
    if(callback_){
        callback_(&loop);
    }
{
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ =  &loop;
    cond_.notify_one();
}
    loop.loop();
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr;
}