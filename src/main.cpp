#include <iostream>
#include "threadpool.h"
#include <thread>
#include <chrono>

/*
多线程 算一下 1+...+ 3亿的和
为喂个线程分配区间计算
*/

using uLong = unsigned long long;

// 重写任务执行逻辑
class MyTask : public Task
{
public:
    MyTask(int a, int b) : a_(a), b_(b) {}


    // 问题一: 怎么返回任意类型的值?
    // c++17 提供了 std::any, 但是不支持c++14
    // 但是 我们不用, 我们自己实现, 看看原理

    Any run() override
    {
        std::cout << "任务开始--thread: " << std::this_thread::get_id() << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(3));

        uLong sum = 0;
        for (uLong i = a_; i <= b_; ++i)
        {
            sum += i; // 计算从a到b的和
        }


        std::cout << "任务结束--thread: " << std::this_thread::get_id() << std::endl;

        return sum;
    }

private:
    int a_; // 任务参数1
    int b_; // 任务参数2
};

int main()
{
    {
        ThreadPool pool;
        pool.setMode(PoolMode::MODE_CACHED); 
        pool.start(2);
        Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
         Result res3 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
        pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));

        pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
        // pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));

        // uLong sum1 = res1.get().cast_<uLong>();

        // std::cout << "计算结果: " << sum1 << std::endl; // 输出计算结果
    }
    std::cout << "所有任务已提交" << std::endl;
    getchar(); 

#if 0
    {
        ThreadPool pool;

        pool.setMode(PoolMode::MODE_CACHED); // 设置线程池模式为动态变化线程池

        pool.start(4); // 启动线程池，初始线程数为4

        // 要实现 线程分区计算, 并拿到每个线程执行结果, 肯定需要接受 每个任务结果
        Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
        Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
        Result res3 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
        pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));

        pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
        pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));

        // Any: Result.get()  用户拿到返回值
        // Any.cast_()  获取存储的数据的类型   也算是对应 用户需要的类型
        uLong sum1 = res1.get().cast_<uLong>();
        uLong sum2 = res2.get().cast_<uLong>();
        uLong sum3 = res3.get().cast_<uLong>();
        uLong totalSum = sum1 + sum2 + sum3; // 计算总和
        std::cout << "总和: " << totalSum << std::endl; // 输出总和

        // 提交多个任务到线程池
        // 测试 3个  11个  任务

        // pool.submitTask(std::make_shared<MyTask>());
        // pool.submitTask(std::make_shared<MyTask>());
        // pool.submitTask(std::make_shared<MyTask>());
        // pool.submitTask(std::make_shared<MyTask>());
        // pool.submitTask(std::make_shared<MyTask>());
        // pool.submitTask(std::make_shared<MyTask>());
        // pool.submitTask(std::make_shared<MyTask>());
        // pool.submitTask(std::make_shared<MyTask>());
        // pool.submitTask(std::make_shared<MyTask>());
        // pool.submitTask(std::make_shared<MyTask>());

        // std::this_thread::sleep_for(std::chrono::seconds(5)); // 等待2秒，模拟任务执行
        std::cout << "所有任务已提交" << std::endl;
    }
    // 阻塞,保证持续运行
    getchar(); // 等待用户输入，防止程序提前结束

#endif
}