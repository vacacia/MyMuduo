#include "Thread.h"
#include "CurrentThread.h"
#include <semaphore.h>

std::atomic_int Thread::numCreated_(0);

Thread::Thread(ThreadFunc func, const std::string &name)
    : started_(false)
    , joined_(false)
    , tid_(0)
    , func_(std::move(func))
    , name_(name)
    {
        setDefaultName();
    }

Thread::~Thread(){
    if(started_ && !joined_){
        thread_->detach();
    }
}

// 一个Thread对象,记录的T就是一个新线程的详细信息
void Thread::start(){
    started_ = true;
    sem_t sem;
    sem_init(&sem, false, 0);

    // 用unique_ptr会更好
    thread_ = std::make_shared<std::thread>([&](){
        tid_ = CurrentThread::tid();
        sem_post(&sem);
        func_();
    });

    // 必须等待获取上面创建的线程的tid值
    sem_wait(&sem);
}


void Thread::join(){
    joined_ = true;
    thread_->join();
}


void Thread::setDefaultName(){
    int num = ++numCreated_;
    if(name_.empty()){
        char buf[32] = {0};
        snprintf(buf,sizeof buf, "Thread%d",num);
        name_ = buf;
    }
}

