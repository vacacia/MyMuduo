#pragma once

#include "Poller.h"
#include "Timestamp.h"

#include <vector>
#include <sys/epoll.h>

class EPollPoller : public Poller{
public:
    EPollPoller(EventLoop *loop);
    ~EPollPoller() override;
    
    Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;
    void updateChannel(Channel *channel) override;
    void removeChannel(Channel *channel) override;

private:
    // EventList 初始大小
    static const int kInitEventListSize = 16;
    
    // 填写活跃的连接
    void fillActiveChannels(int numEvents, ChannelList *activeChannnels) const;
    // 更新channel通道
    void update(int operation, Channel *channel);

    using EventList = std::vector<epoll_event>;

    int epollfd_;
    EventList events_;
};