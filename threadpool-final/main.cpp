#include <iostream>
#include "threadpool.h"
#include <thread>
#include <chrono>
#include <future>

/*
多线程 算一下 1+...+ 3亿的和
为喂个线程分配区间计算
*/

using uLong = unsigned long long;

uLong sum1(uLong start, uLong end)
{
    std::this_thread::sleep_for(std::chrono::seconds(2)); // 模拟耗时操作
    return start + end;
}

uLong sum2(uLong a, uLong b, uLong c)
{
        std::this_thread::sleep_for(std::chrono::seconds(2)); // 模拟耗时操作

    return a + b + c;
}


int main()
{
    ThreadPool pool;
    // pool.setMode(PoolMode::MODE_CACHED); // 设置线程池模式为可变线程池
    pool.start(2); // 启动线程池，初始线程数为4


    std::future<uLong> r1= pool.submitTask(sum1, 1,2);
    std::future<uLong> r2= pool.submitTask(sum2, 1, 2, 3);
    std::future<uLong> r3= pool.submitTask(
        [](uLong a, uLong b) { return a + b; }, 4, 5
    );
    std::future<uLong> r4= pool.submitTask(
        [](uLong a, uLong b) { return a + b; }, 5, 5
    );
    std::future<uLong> r5= pool.submitTask(
        [](uLong a, uLong b) { return a + b; }, 6, 5
    );
    std::future<uLong> r6= pool.submitTask(
        [](uLong a, uLong b) { return a + b; }, 7, 5
    );

    std::cout << "sum1: " << r1.get() << std::endl; // 获取任务结果
    std::cout << "sum2: " << r2.get() << std::endl; // 获取任务结果
    std::cout << "sum3: " << r3.get() << std::endl; // 获取任务结果
    std::cout << "sum4: " << r4.get() << std::endl; // 获取任务结果
    std::cout << "sum5: " << r5.get() << std::endl; // 获取任务结果
    std::cout << "sum6: " << r6.get() << std::endl; // 获取任务结果

    getchar(); // 等待输入, 保持控制台窗口不关闭

}