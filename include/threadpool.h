#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>

// 线程池模式
enum class PoolMode
{
    MODE_FIXED,  // 固定线程池
    MODE_CACHED, // 动态变化线程池
};

// 抽象任务基类
class Task
{
public:
    virtual void run() = 0;

private:
};

// 线程类型
class Thread
{
public:
    // 启动线程
    void start();

private:
};

// 线程池类型
class ThreadPool
{
public:
    ThreadPool();
    ~ThreadPool();

    // 设置线程池模式
    void setMode(PoolMode mode);

    // // 设置task队列最大线程数
    // void setTaskQueMaxThreadSize(int size);   // 优化掉, start直接传入

    // 提交任务到线程池
    void submitTask(std::shared_ptr<Task> sp);

    // 开启线程池
    void start(int initThreadSize = 4);

    // 禁止拷贝和赋值
    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;

private:
    // 定义线程函数
    void threadFunc();

private:
    std::vector<Thread *> threads_; // 线程列表
    size_t initThreadSize_;         // 初始线程数量

    std::queue<std::shared_ptr<Task>> taskQue_; // 任务队列
    std::atomic_uint taskSize_;                 // 任务数量  线程安全
    int taskQueMaxThreadSize_;                  // 任务队列最大线程数, 阈值

    std::mutex taskQueMutex_;          // 任务队列互斥锁
    std::condition_variable notFull_;  // 任务队列不满
    std::condition_variable notEmpty_; // 任务队列不空

    PoolMode poolmode_; // 当前线程池模式
};

#endif // THREADPOOL_H