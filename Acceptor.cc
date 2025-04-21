#include "Acceptor.h"
#include "Logger.h"
#include "InetAddress.h"

#include <sys/types.h>    
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>

static int createNonblocking(){
    //LOG_INFO("Acceptor-createNonBlocking");
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if(sockfd < 0){
        LOG_FATAL("%s:%s:%d lsiten socket create err:%d \n", __FILE__, __FUNCTION__, __LINE__, errno);
    }
    //LOG_INFO("Acceptor-createNonBlocking - sock = %d", sockfd);
    return sockfd;
}

Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport)
    : loop_(loop)
    , acceptSocket_(createNonblocking())
    , acceptChannel_(loop, acceptSocket_.fd())
    , listenning_(false)
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(reuseport);
    acceptSocket_.bindAddress(listenAddr);
    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}  


Acceptor::~Acceptor(){
    acceptChannel_.disableAll();
    acceptChannel_.remove();

}

void Acceptor::listen(){
    //LOG_INFO("Acceptor-listen");
    listenning_ = true;
    acceptSocket_.listen();
    acceptChannel_.enableReading();

}
void Acceptor::handleRead(){
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    if(connfd >= 0){
        LOG_INFO("Acceptor::handleRead - New connection from %s", peerAddr.toIpPort().c_str()); // 添加日志
        if(newConnectionCallback_){
            newConnectionCallback_(connfd, peerAddr);
        }else{
            ::close(connfd);
        }
    }else{
        LOG_ERROR("%s:%s:%d accept err :%d \n", __FILE__, __FUNCTION__, __LINE__, errno);
        if(errno == EMFILE){
            LOG_ERROR("%s:%s:%d sockfd eached limit! \n", __FILE__, __FUNCTION__, __LINE__);
        }
    }
}