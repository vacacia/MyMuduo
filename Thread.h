#pragma once

#include "noncopyable.h"

#include <memory>
#include <atomic>
#include <string>
#include <thread>
#include <functional>

class Thread:noncopyable{
public:
    using ThreadFunc = std::function<void()>;
    explicit Thread(ThreadFunc, const std::string &name = std::string());

    ~Thread();

    void start();
    void join();

    bool started() const { return started_; }
    pid_t tid() const { return tid_; }
    const std::string& name() const { return name_; }
    // 静态成员函数上不允许使用类型限定符
    static int numCreated() { return numCreated_; }

private:
    void setDefaultName();
    bool started_;
    bool joined_;
    std::shared_ptr<std::thread> thread_;
    pid_t tid_;
    ThreadFunc func_;
    std::string name_;
    static std::atomic_int numCreated_;
};