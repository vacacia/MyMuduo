#pragma once

#include <sys/syscall.h>
#include <unistd.h>

namespace CurrentThread {
// __thread 是 GCC 提供的线程本地存储（TLS）关键字，用于声明线程局部变量
extern __thread int t_cachedTid;

void cacheTid();

inline int tid() {
  if (__builtin_expect(t_cachedTid == 0, 0)) {
    cacheTid();
  }
  return t_cachedTid;
}

} // namespace CurrentThread