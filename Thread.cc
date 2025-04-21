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
    if(started_ && joined_){
        // 将线程设置为分离状态
        // 当一个线程被设置为分离状态后，它将独立于创建它的线程运行
        // 并且在其执行完毕后，会自动释放相关资源，无需调用 join() 来等待它结束。
        thread_->detach();
    }
}

// 一个Thread对象,记录的T就是一个新线程的详细信息
void Thread::start(){
    started_ = true;
    sem_t sem;
    sem_init(&sem, false, 0);

    // 已经改进不使用new：std::shared_ptr<std::thread>(new std::thread(
    // 用unique_ptr会更好吗 
    // 由于thread构造函数可以接受一个可调用对象，所以可以lambda、智能指针、thread配合
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
    // join是std::thread类提供的，它会阻塞当前调用线程, 直到 thread_ 执行完毕
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

