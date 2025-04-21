#include "TcpConnection.h"
#include "EventLoop.h"
#include "Socket.h"
#include "Channel.h"
#include "Logger.h"
#include "Buffer.h"

#include <functional>
#include <errno.h> // errno
#include <string.h> // strerror
#include <sys/types.h> // ssize_t
#include <sys/socket.h> // write
#include <unistd.h> // close
#include <string> 


static EventLoop *CheckLopNotNull(EventLoop *loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d mainLoop is null\n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpConnection::TcpConnection(
    EventLoop *loop,
    const std::string &name,
    int sockfd,
    const InetAddress &localAddr,
    const InetAddress &peerAddr) : loop_(CheckLopNotNull(loop)),
                                   name_(name),
                                   state_(kConnecting),
                                   reading_(true),
                                   socket_(new Socket(sockfd)),
                                   channel_(new Channel(loop, sockfd)),
                                   localAddr_(localAddr),
                                   peerAddr_(peerAddr),
                                   highWaterMark_(64 * 1024 * 1024) // 64M
{
    channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(std::bind(&TcpConnection::handleError, this));

    LOG_INFO("TcpConnection::ctor[%s] at fd=%d\n", name_.c_str(), sockfd);
    // 设置 socket 的 keepalive 选项
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
    LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d\n", 
        name_.c_str(), channel_->fd(), (int)state_);
}

void TcpConnection::send(const std::string &msg)
{
    if(state_ == kConnected){
        if(loop_->isInLoopThread()){
            sendInLoop(msg.c_str(), msg.size());
        }else{
            loop_->runInLoop(std::bind(&TcpConnection::sendInLoop, this, msg.c_str(), msg.size()));
        }
    }
}

// 流程：
// 1.检查是否发送数据条件，符合则直接发送数据
// 2.发送成功，回调writeCompleteCallback_
// 3.发送失败，记录错误信息，非致命错误与 1.里 不符合条件的情况一起处理
// ---
// 4.未发送完和发送失败的情况，都会注册写事件，等待下一次发送
void TcpConnection::sendInLoop(const void* data, size_t len){
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;

    // 之前调用过该connection的shutdown函数，不能再发送了
    if(state_ == kDisconnected){
        LOG_ERROR("disconnected, give up writing\n");
        return;
    }

    // 当前没有待发送的旧数据（outBuffer_可读为空），且未注册写事件，直接发送
    if(!channel_->isWriting() && outputBuffer_.readableBytes() == 0){
        nwrote = ::write(channel_->fd(), data, len);
        if(nwrote >= 0){
            remaining = len - nwrote;
            // 全部发送成功，不用注册写事件，直接回调writeCompleteCallback_
            if(remaining == 0 && writeCompleteCallback_){
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }else{ //一个都没发出去/发送失败
            nwrote = 0;
            if(errno != EWOULDBLOCK){
                LOG_ERROR("TcpConnection::senINLoop");
                // EPIPE: 对端关闭连接
                // ECONNRESET: 对端重置连接
                if(errno == EPIPE || errno == ECONNRESET){
                    faultError = true;
                }
            }
        }
    }

    // 未全部发送
    // 剩余的数据保存到缓冲区，给channel注册epollout事件。
    // 待poller发现TCP发送缓冲区有空间，会通知相应的channel
    // channel调用writeCallback_ <- TcpConnection::handlewrite(),处理发送缓冲区里的数据
    if(!faultError && remaining > 0){
        size_t oldLen = outputBuffer_.readableBytes();
        if(oldLen + remaining >= highWaterMark_ // 总数据量超过高水位线
            && oldLen < highWaterMark_ // 旧数据未超（确保是首次超过水位线，避免重复触发）
            && highWaterMark_){
                loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldLen+remaining));
        }
        outputBuffer_.append(static_cast<const char*>(data) + nwrote, remaining);
        if(!channel_->isWriting()){
            channel_->enableWriting();
        }
    }

}
void TcpConnection::shutdown()
{
    if(state_ == kConnected){
        setState(kDisconnected);
        loop_->runInLoop(
            std::bind(&TcpConnection::shutdownInLoop, this)
        );
    }
}

void TcpConnection::shutdownInLoop()
{
    // 此时，outputBuffer_中的数据已经发送完毕
    if(!channel_->isWriting()){
        socket_->shutdownWrite();
    }
}

void TcpConnection::connectEstablished()
{
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading();

    connectionCallback_(shared_from_this());
}
void TcpConnection::connectDestroyed()
{
    if(state_ == kConnected){
        setState(kDisconnected);
        channel_->disableAll();
        connectionCallback_(shared_from_this());

    }
    channel_->remove();
}

void TcpConnection::handleRead(Timestamp receiveTime)
{
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
    if(n>0){
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }else if(n == 0){
        handleClose();
    }else{
        errno = savedErrno;
        LOG_ERROR("TcpConnection::handleRead");
        handleError();
    }
}

//当内核的 TCP 发送缓冲区有空间（即可以继续写入数据）时，
//事件循环（EventLoop）会通过 EPOLLOUT 事件触发 handleWrite。
// 任务：将 outputBuffer_ 中缓存的数据发送到内核。
void TcpConnection::handleWrite()
{
    if(channel_->isWriting()){
        int savedErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &savedErrno);
        if(n > 0){
            outputBuffer_.retrieve(n);
            if(outputBuffer_.readableBytes() == 0){
                channel_->disableWriting();
                if(writeCompleteCallback_){
                    loop_->queueInLoop(
                        std::bind(writeCompleteCallback_, shared_from_this())
                    );
                }
                if(state_ == kDisconnecting){
                    shutdownInLoop();
                }
            }
        }else{// n <= 0
            LOG_ERROR("TcpConnection::handleWrite");
        }
    }else{// !channel_->isWriting()
        LOG_ERROR("Connection fd=%d is down, no more writing\n", channel_->fd());

    }
}
void TcpConnection::handleClose()
{
    LOG_INFO("TcpConnection::handleClose fd=%d state=%d \n", channel_->fd(), (int)state_);
    setState(kDisconnected);
    channel_->disableAll();

    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr);
    closeCallback_(connPtr);

}
void TcpConnection::handleError()
{   
    int optval;
    socklen_t optlen = sizeof optval;
    int err = 0;
    if(::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0){
        err = errno;
    }else{
        err = optval;
    }
    LOG_ERROR("TcpConnection::handleEooro name:%s - SO_ERROR:%d \n",name_.c_str(), err);
}

