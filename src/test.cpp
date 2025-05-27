#include <iostream>
#include "threadpool.h"
#include <thread>
#include <chrono>

int main()
{
    ThreadPool pool;
    pool.start(10); // 启动线程池，初始线程数为4

    
    std::this_thread::sleep_for(std::chrono::seconds(5)); // 等待2秒，模拟任务执行
    
}