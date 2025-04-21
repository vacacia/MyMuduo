#pragma once

#include <iostream>

#include <string>

class Timestamp{
public:
    Timestamp();
    explicit Timestamp(int64_t microSecondsSinceEpoch);
    static Timestamp now();
    std::string toString();
private:
    int64_t microSecondsSinceEpoch_;
};