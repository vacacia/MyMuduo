#pragma once

#include "noncopyable.h"
#include "EventLoop.h"
#include "Acceptor.h"
#include "EventLoopThreadPool.h"
#include "InetAddress.h"
#include "Callbacks.h"
#include "Buffer.h"
#include "TcpConnection.h"


#include <functional>
#include <string>
#include <memory>
#include <atomic>
#include <unordered_map>



// 对外的服务器编程使用的类
class TcpServer : noncopyable{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    enum Option{
        kNoReusePort,
        kReusePort,
    };

    TcpServer(EventLoop *loop, 
                const InetAddress &listenAddr, 
                const std::string &nameArg,
                Option option = kNoReusePort);
    ~TcpServer();

    void setThreadinitCallback(const ThreadInitCallback &cb){
        threadInitCallback_ = cb;
    }


    void setThreadInitCallback(const ThreadInitCallback &cb) { threadInitCallback_ = cb; }
    void setConnectionCallback(const ConnectionCallback &cb) { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback &cb) { writeCompleteCallback_ = cb; }


    // 设置底层subloop的个数
    void setThreadNum(int numThreads);

    // 开启服务器监听
    void start();
private:
    // 新连接到来时的回调
    void newConnection(int sockfd, const InetAddress &peerAddr);
    // 连接断开时的回调
    void removeConnection(const TcpConnectionPtr &conn);
    void removeConnectionInLoop(const TcpConnectionPtr &conn);

    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

    // baseloop,用户定义的loop
    EventLoop* loop_;

    const std::string ipPort_;
    const std::string name_;

    // 运行在mainLoop，监听连接事件
    std::unique_ptr<Acceptor> acceptor_;

    std::shared_ptr<EventLoopThreadPool> threadPool_;

    // 有新连接时的回调
    ConnectionCallback connectionCallback_;
    // 有读写消息时的回调
    MessageCallback  messageCallback_;
    // 消息发送完的回调
    WriteCompleteCallback writeCompleteCallback_;

    //loop 线程初始化的回调
    ThreadInitCallback threadInitCallback_;

    std::atomic_int started_;

    int nextConnId_;
    ConnectionMap connections_;
};
