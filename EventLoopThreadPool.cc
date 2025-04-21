#include "EventLoopThreadPool.h"
#include "EventLoopThread.h"

EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg)
    : baseLoop_(baseLoop)
    , name_(nameArg)
    , started_(false)
    , numThreads_(0)
    , next_(0){
}

EventLoopThreadPool::~EventLoopThreadPool(){
}

void EventLoopThreadPool::start(const ThreadInitCallback &cb){
    started_ = true;
    for(int i = 0 ; i < numThreads_ ; ++i){
        char buf[name_.size() + 32];
        // name.c_str()
        snprintf(buf, sizeof buf, "%s%d", name_.c_str(), i);
        EventLoopThread *t = new EventLoopThread(cb, buf);
        threads_.push_back(std::unique_ptr<EventLoopThread>(t));
        // startLoop创建事件循环，并返回指向该事件循环的指针
        loops_.push_back(t->startLoop());
    }
    // 没有额外的工作线程，并且提供了有效的线程初始化回调函数cb，
    // 调用该回调函数并传入 baseLoop_ 指针，以便在 baseLoop_ 上进行初始化
    if(numThreads_ == 0 && cb){
        cb(baseLoop_);
    }
}


// 如果没有设置多个线程，只有主线程，返回的loop就是主线程
EventLoop* EventLoopThreadPool::getNextLoop(){
    EventLoop *loop = baseLoop_;

    if(!loops_.empty()){
        loop = loops_[next_];
        ++next_;
        if(next_ >= loops_.size()){
            next_ = 0;
        }
    }
    return loop;
}

std::vector<EventLoop*> EventLoopThreadPool::getAllLoops(){
    if(loops_.empty()){
        // loops_ 起始为空
        return std::vector<EventLoop*>(1, baseLoop_);
    }
    else{
        return loops_;
    }
}