#pragma once

#include <vector>
#include <string>

class Buffer
{
public:
    // 前置区域的大小
    static const size_t kCheapPrepend = 8;
    // 初始缓冲区大小
    static const size_t kInitialSize = 1024;

    explicit Buffer(size_t initialSize = kInitialSize):
        buffer_(kCheapPrepend + kInitialSize),
        readerIndex_(kCheapPrepend),
        writerIndex_(kCheapPrepend)
    {}


    size_t readableBytes() const{
        return writerIndex_ - readerIndex_;
    }
    size_t writableBytes() const{
        return buffer_.size() - writerIndex_;
    }
    // 返回缓冲区中前置区域的长度，前置区域是指当前可读数据之前的那部分空间
    size_t prependableBytes() const{
        return readerIndex_;
    }
    
    // 计算出可读数据区域的第一个字节的内存地址
    const char* peek() const{
        return begin() + readerIndex_;
    }

    // 从缓冲区读取长度为len的数据
    void retrieve(size_t len){
        if(len < readableBytes()){
            readerIndex_ += len;
        }else{
            retrieveAll();
        }
    }
    void retrieveAll(){
        readerIndex_ = kCheapPrepend;
        writerIndex_ = kCheapPrepend;
    }
    
    std::string retrieveAsString(size_t len){
        std::string result(peek(), len);
        retrieve(len);
        return result;
    }
    std::string retrieveAllAsString(){
        return retrieveAsString(readableBytes());
    }

    void ensureWriteableBytes(size_t len){
        if(writableBytes() < len){
            makeSpace(len);
        }
    }

    void append(const char* data, size_t len){
        ensureWriteableBytes(len);
        // copy的作用是将data指向的数据拷贝到缓冲区中
        // copy的第一个参数是数据的起始地址
        // copy的第二个参数是数据的结束地址
        // copy的第三个参数是数据的目的地址
        std::copy(data, data+len, beginWrite());
    }

    // 从fd中读取数据到缓冲区
    ssize_t readFd(int fd, int* saveErrno);
    // 从缓冲区中读取数据到fd
    ssize_t writeFd(int fd, int* saveErrno);
private:
    // 返回缓冲区的起始地址
    char* begin(){
        return &(*(buffer_.begin()));
    }

    // 不能删掉非const版本，有时需要修改缓冲区，需要可修改的指针
    // 可以通过const_cast<>来转换，非 const 版本复用 const 版本
    const char* begin() const {
        return &(*(buffer_.begin()));
    }

    char* beginWrite(){
        return begin() + writerIndex_;
    }
    const char* beginWrite() const{
        return begin() + writerIndex_;
    }

    // 扩容缓冲区
    void makeSpace(size_t len);

    // [readerIndex_, writerIndex_)是缓冲区中的有效数据
    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};