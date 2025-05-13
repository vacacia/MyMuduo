#pragma once

#include "noncopyable.h"
#include "InetAddress.h"
#include "Callbacks.h"
#include "Buffer.h"
#include "Timestamp.h"

#include <atomic>
#include <string>
#include <memory>

class Socket;
class Channel;
class EventLoop;

class TcpConnection : noncopyable,
    // 当一个类继承自std::enable_shared_from_this时，它可以安全地生成指向自身的std::shared_ptr实例。
    public std::enable_shared_from_this<TcpConnection>
{
public:
    TcpConnection(
        EventLoop *loop,
        const std::string &name, 
        int sockfd,
        const InetAddress& localAddr,
        const InetAddress& peerAddr
    );
    ~TcpConnection();

    EventLoop* getLoop() const { return loop_; }
    const std::string &name() const { return name_; }
    const InetAddress& localAddress() const { return localAddr_; }
    const InetAddress& peerAddress() const { return peerAddr_; }

    bool connected() const { return state_ == kConnected; }

    void send(const std::string &buf);
    void shutdown();

    void setConnectinCallback(const ConnectionCallback &cb) {
        connectionCallback_ = cb;
    }
    void setMessageCallback(const MessageCallback &cb) {
        messageCallback_ = cb;
    }
    void setWriteCompleteCallback(const WriteCompleteCallback &cb) {
        writeCompleteCallback_ = cb;
    }
    void setHighWaterMarkCallback(const HighWaterMarkCallback &cb, size_t highWaterMark) {
        highWaterMarkCallback_ = cb;
        highWaterMark_ = highWaterMark;
    }
    void setCloseCallback(const CloseCallback &cb) {
        closeCallback_ = cb;
    }

    void connectEstablished();
    void connectDestroyed();


private:
    enum StateE {
        kDisconnected,
        kConnecting,
        kConnected,
        kDisconnecting
    };
    void setState(StateE state) { state_ = state;}

    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();

    void sendInLoop(const void* message, size_t len);
    void shutdownInLoop();




    // 私有属性
    // 这里绝不是baseloop，因为TcpConnection都是在subloop中创建的
    EventLoop* loop_;
    std::string name_;
    std::atomic_int state_;
    bool reading_;

    // 这里和Acceptor类似，Acceptor -> mainloop, TcpConnection -> subloop
    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    // 回调函数
    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    HighWaterMarkCallback highWaterMarkCallback_;
    CloseCallback closeCallback_;
    size_t highWaterMark_;

    // 缓冲区
    Buffer inputBuffer_;
    Buffer outputBuffer_;
};