#include "Timestamp.h"
#include <time.h>

Timestamp::Timestamp():microSecondsSinceEpoch_(0){}

Timestamp::Timestamp(int64_t microSecondsSinceEpoch)
    :microSecondsSinceEpoch_(microSecondsSinceEpoch)
    {}

Timestamp Timestamp::now(){
    return Timestamp(time(NULL));
}

std::string Timestamp::toString(){
    char timeStr[128] = {0};
    // localtime 接收一个指向 time_t 类型变量的指针
    // time_t 用来存储时间，通常表示自 Epoch 以来经过的秒数。
    tm *tm_time = localtime(&microSecondsSinceEpoch_);
    snprintf(timeStr, 128, "%4d/%02d/%02d %02d:%02d:%02d",
        tm_time->tm_year + 1900,
        tm_time->tm_mon + 1,
        tm_time->tm_mday,
        tm_time->tm_hour,
        tm_time->tm_min,
        tm_time->tm_sec
    );
    return timeStr;
}

// #include <iostream>
// int main(){
//     std::cout << Timestamp::now().toString() << std::endl;
//     return 0;
// }