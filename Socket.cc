#include "Socket.h"
#include "Logger.h"
#include "InetAddress.h"

#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <strings.h>
#include <cstring>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

Socket::~Socket(){
    ::close(sockfd_);
}

void Socket::bindAddress(const InetAddress &localaddr){
    if (0 != ::bind(sockfd_, (sockaddr*)localaddr.getSockAddr(), sizeof(sockaddr_in))){
        LOG_FATAL("bind sockfd: %d fail, errno: %d, error: %s\n", sockfd_, errno, strerror(errno));
    }
}

void Socket::listen(){
    if(0 != ::listen(sockfd_, 1024)){
        LOG_FATAL("listen fd : %d fail\n", sockfd_);
    }
    //LOG_INFO("socket-listen");
}

int Socket::accept(InetAddress *peeraddr){
    sockaddr_in addr;
    socklen_t len = sizeof addr;
    bzero(&addr, len);
    
    int connfd = ::accept4(sockfd_, (sockaddr*)&addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (connfd >= 0){
        peeraddr->setSockAddr(addr);
    }
    return connfd;
}

void Socket::shutdownWrite(){
    if(::shutdown(sockfd_, SHUT_WR) < 0){
        LOG_ERROR("shutdownWrite fail! \n");
    }
}

void Socket::setTcpNoDelay(bool on){
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof optval);
}
void Socket::setReuseAddr(bool on){
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
}
void Socket::setReusePort(bool on){
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof optval);
}

void Socket::setKeepAlive(bool on){
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof optval);
}

