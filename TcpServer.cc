#include "TcpServer.h"
#include "Logger.h"
#include "EventLoop.h"
#include "TcpConnection.h"

#include <functional>
#include <memory>
#include <string>
#include <strings.h>

static EventLoop* CheckLoopNotNull(EventLoop* loop){
    if (loop == nullptr){
        LOG_FATAL("%s:%s:%d mainLoop is null! \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpServer::TcpServer(EventLoop *loop, 
    const InetAddress &listenAddr, 
    const std::string &nameArg,
    Option option)
    :loop_(CheckLoopNotNull(loop)),
    ipPort_(listenAddr.toIpPort()),
    name_(nameArg),
    acceptor_(new Acceptor(loop, listenAddr, option == kReusePort)),
    threadPool_(new EventLoopThreadPool(loop, name_)),
    connectionCallback_(),
    messageCallback_(),
    nextConnId_(1),
    started_(0)
{
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this,
        std::placeholders::_1, std::placeholders::_2));
}

TcpServer::~TcpServer(){
    for (auto &item : connections_){
        TcpConnectionPtr conn(item.second);

        //reset() 是 std::shared_ptr 类的一个成员函数
        //reset() 会将当前指针指向的对象引用计数减一，如果引用计数为0，则释放对象
        item.second.reset();

        
        conn->getLoop()->runInLoop(
            std::bind(&TcpConnection::connectDestroyed, conn)
        );

    }
}

// 设置底层subloop的个数
void TcpServer::setThreadNum(int numThreads){
    threadPool_->setThreadNum(numThreads);
}

// 开启服务器监听 loop.loop()
void TcpServer::start(){
    if(started_++ == 0){
        threadPool_->start(threadInitCallback_);
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
    }
}

// 新客户端连接，acceptor执行这个回调
void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr){
    // 1. 选择一个 subLoop 处理新连接
    EventLoop *ioLoop = threadPool_->getNextLoop();

    // 2. 生成连接名称
    char buf[64] = {0};
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;

    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s \n",
    name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());

    // 3. 通过sockfd获取本地地址信息
    sockaddr_in local;
    ::bzero(&local, sizeof local);
    socklen_t addrlen = sizeof local;
    if(::getsockname(sockfd, (sockaddr*)&local, &addrlen) < 0){
        LOG_ERROR("sockets::getLocalAddr");
    }
    InetAddress localaddr(local);

    // 4. 创建新连接 - TcpConnection 对象
    TcpConnectionPtr conn(
        new TcpConnection(
            ioLoop, 
            connName, 
            sockfd, 
            localaddr, 
            peerAddr
        )
    );
    connections_[connName] = conn;

    // 5. 设置连接回调
    // 下面的回调都是用户设置给TcpServer=>TcpConnection=>Channel=>Poller=>notify channel调用回调
    conn->setConnectinCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    conn->setCloseCallback(
        std::bind(&TcpServer::removeConnection, this, std::placeholders::_1)
    );

    ioLoop->runInLoop(
        std::bind(&TcpConnection::connectEstablished, conn)
    );
}


void TcpServer::removeConnection(const TcpConnectionPtr &conn){
    loop_->runInLoop(
        std::bind(&TcpServer::removeConnectionInLoop, this, conn)
    );
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn)
{
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection %s\n", 
        name_.c_str(), conn->name().c_str());

    connections_.erase(conn->name());
    EventLoop *ioLoop = conn->getLoop(); 
    ioLoop->queueInLoop(
        std::bind(&TcpConnection::connectDestroyed, conn)
    );
}