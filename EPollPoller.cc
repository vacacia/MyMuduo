#include "EPollPoller.h"
#include "Logger.h"
#include "Channel.h"

#include <errno.h>
#include <unistd.h>
#include <string.h>

// channel未添加到poller中
const int kNew = -1; // channel 的 indx_ = -1 -->表示状态
// channel已添加到poller中
const int kAdded = 1;
//channel从poller中删除
const int kDeleted = 2;

EPollPoller::EPollPoller(EventLoop *loop)
    : Poller(loop) // 调用基类的构造函数，初始化基类的量
    , epollfd_(epoll_create1(EPOLL_CLOEXEC)) 
    , events_(kInitEventListSize){
    if(epollfd_ < 0){
        LOG_FATAL("epoll create error:%d\n", errno);
    }
}

EPollPoller::~EPollPoller(){
    ::close(epollfd_);
}

// 提供 epoll_wait 监听哪些 channel 发生了事件，并通过 activeChannels 通知 EventLoop
Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannels) {
    // 因为执行频繁，LOG_DEBUG合适
    // %d - int, %u - unsigned int,  %lu - long unsigned int(size_t)
    LOG_DEBUG("func=%s => fd TOTAL count=%lu\n", __FUNCTION__, channels_.size());
    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(), static_cast<int>(events_.size()), timeoutMs);
    int saveErrno = errno;
    Timestamp now(Timestamp::now());

    if (numEvents > 0) {
        LOG_INFO("%d events happend \n", numEvents);
        fillActiveChannels(numEvents, activeChannels);
        //LOG_INFO("fillActiveChannels %s", __FUNCTION__);
        if (numEvents >= events_.size()){
            events_.resize(events_.size() * 2);
        }
    }else if(numEvents == 0){
        // 没有事件发生，只是超时
        LOG_DEBUG("nothing happened, %s timeout! \n", __FUNCTION__);
    }else{
        // 不是外部中断，就继续处理
        if (saveErrno != EINTR){
            // 把刚才的全局变量errno暂存到局部变量，因为全局变量errno可能被其他人修改
            // 需要输出errno时再改回来
            errno = saveErrno;
            LOG_ERROR("EPoller::poll() err!");
        }
    }
    return now;
}
void EPollPoller::updateChannel(Channel *channel) {
    const int index = channel->index();
    // LOG_INFO("func = %s, fd = %d, events = %d, index = %d", __FUNCTION__, channel->fd(), channel->events(), index);
    LOG_INFO("func=%s => channel_fd=%d channel_events=%d channel_index=%d \n", __FUNCTION__, channel->fd(), channel->events(), index);

    // 偷换了位置，可能有bug
    // int fd = channel->fd();

    // 
    if(index == kNew || index == kDeleted){
        // 新加入的channel
        if(index == kNew){
            int fd = channel->fd();
            LOG_INFO("Adding new channel with fd=%d to channels_", fd); 
            channels_[fd] = channel;
        }
        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel);
    }else{ // channel == kAdded, update existing one with EPOLL_CTL_MOD/DEL
        if(channel->isNoneEvent()){
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        }else{ 
            update(EPOLL_CTL_MOD, channel);
        }
    }

}

// 从Poller里删除, 不过删除后还存在于Poller的channels里
// 删除前，保证 
// 1. 对于Poller：fd 在channels(poller的一个map)里存在
// 2. 对于Poller：fd 对应的 channel 类型无误
// 3. 对于Channel：channel isNoneEvent 即 events_ == kNoneEvent;
// 4. 对于Channel：index(状态) 已被改为 已添加/已删除
// 删除后
// 重置channel的状态为kNew，
// EpollPoller调用epoll的EPOLL_CTL_DEL来删除fd的事件监听
// Poller的channels也会删除fd-channel键值对
void EPollPoller::removeChannel(Channel *channel) {
    int fd = channel->fd();
    channels_.erase(fd);

    
    LOG_INFO("func = %s, fd = %d", __FUNCTION__, fd);

    int index = channel->index();
    if(index == kAdded){
        update(EPOLL_CTL_DEL, channel);
    }
    channel->set_index(kNew);
}

void EPollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannnels) const {
    //LOG_INFO("EPollPoller::fillActiveChannels");
    for(int i = 0; i < numEvents; ++i){
        // events_[i].data.ptr要加括号
        //LOG_INFO("EPollPoller::fillActiveChannels 1");
        Channel *channel = static_cast<Channel*>(events_[i].data.ptr);
        // events_ 是epoll_event的vector，
        //LOG_INFO("EPollPoller::fillActiveChannels 2");
        //LOG_INFO("events_[i].events = %d", events_[i].events);
        // 第二次events_[i].events = 0，然后Segmentation fault
        channel->set_revents(events_[i].events);
        //LOG_INFO("EPollPoller::fillActiveChannels 3");
        activeChannnels->push_back(channel);
    }
    //LOG_INFO("EPollPoller::fillActiveChannels end");
}



// 更新channel
void EPollPoller::update(int operation, Channel *channel) {
    epoll_event event;
    memset(&event, 0, sizeof event);

    int fd = channel->fd();
    event.events = channel->events();
    event.data.fd = fd;
    event.data.ptr = channel;
    
    if(epoll_ctl(epollfd_, operation, fd, &event) < 0){
        if(operation == EPOLL_CTL_DEL) {
            LOG_ERROR("epoll_ctl del error:%d\n", errno);
        }else{
            LOG_FATAL("epoll_ctl mod/add error:%d\n", errno);
        }
    }
}