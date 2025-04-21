#include "Buffer.h"

#include <unistd.h> //read write
#include <sys/types.h> //ssize_t
#include <errno.h> //errno
#include <sys/uio.h> //iovec

// 从fd中读取数据到缓冲区
ssize_t Buffer::readFd(int fd, int* saveErrno)
{
    char extrabuf[65536] = {0};

    // iovec是一个结构体，用于在一次函数调用中传递多个缓冲区
    // 通过iovec结构体，可以将多个缓冲区的数据合并成一个数据块进行传输
    struct iovec vec[2];

    const size_t writable = writableBytes();

    // iov_base指向缓冲区的起始地址
    // iov_len指定缓冲区的长度
    vec[0].iov_base = beginWrite();
    vec[0].iov_len = writable;

    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;

    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
    const ssize_t n = ::readv(fd, vec, iovcnt);

    if(n < 0){
        *saveErrno = errno;
    }else if(n <= writable){
        writerIndex_ += n;
    }else{
        // 缓冲区buffer_不够用，已经使用了extrabuf
        // 在append里扩容了buffer_,然后将extrabuf的数据拷贝到buffer_中
        writerIndex_ = buffer_.size();
        // append总是把数据追加到buffer_中，空间不够一定会先扩容
        append(extrabuf, n - writable);
    }
    return n;
    
}
// 从缓冲区中读取数据到fd
ssize_t Buffer::writeFd(int fd, int* saveErrno)
{
    ssize_t n = ::write(fd, peek(), readableBytes());
    if(n < 0){
        *saveErrno = errno;
    }
    return n;
}

void Buffer::makeSpace(size_t len){
    // 总空间不够，整理空间也存不下
    if(writableBytes() + prependableBytes() < len + kCheapPrepend){
        buffer_.resize(writerIndex_ + len);
    }else{ // 整理空间，将可读数据前移
        size_t readable = readableBytes();
        std::copy(begin() + readerIndex_,
            begin() + writerIndex_,
            begin() + kCheapPrepend);
        readerIndex_ = kCheapPrepend;
        writerIndex_ = readerIndex_ + readable;
    }
}