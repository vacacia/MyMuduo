#!/bin/bash

# 任何命令执行失败就立即退出
set -e

# 1.创建目录
if [ ! -d $(pwd)/build ]; then
    mkdir $(pwd)/build
fi

# 2.清理并构建项目
rm -rf $(pwd)/build/*
cd $(pwd)/build &&
    cmake .. &&
    make

# 回到根目录
cd ..

# 3.安装头文件和库
if [ ! -d /usr/include/mymuduo ]; then
    mkdir /usr/include/mymuduo
fi

for header in $(ls *.h)
do
    cp $header /usr/include/mymuduo
done

cp $(pwd)/lib/libmymuduo.so /usr/lib
ldconfig

