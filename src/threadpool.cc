#include "threadpool.h"
#include <functional>
#include <thread>
#include <iostream>

const int TASK_MAX_THRESHOLD = 1024; // 任务队列最大阈值

ThreadPool::ThreadPool()
    : initThreadSize_(4), taskSize_(0), taskQueMaxThreadSize_(0), poolmode_(PoolMode::MODE_FIXED)
{
    // 初始化线程池
}

ThreadPool::~ThreadPool()
{
    // 没有new过, 不用写
}

// 设置线程池模式--手动设置
void ThreadPool::setMode(PoolMode mode)
{
    poolmode_ = mode;
}

// // 设置task队列最大线程数
// void ThreadPool::setTaskQueMaxThreadSize(int size)
// {
//    initThreadSize_ = size > 0 ? size : 1; // 至少为1
// }

// 提交任务到线程池
void ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
    // 获取锁
    std::unique_lock<std::mutex> lock(taskQueMutex_);
    // 线程通信 等待任务队列有空余
    // while(taskQue_.size() == TASK_MAX_THRESHOLD)
    // {
    //     notFull_.wait(lock); // 等待任务队列不满   条件变量,不要搞混信号量
    // }
    // // 优化   lambda条件成立, 会往下走
    // notFull_.wait(lock,  [&] (){
    //     return taskQue_.size() < TASK_MAX_THRESHOLD;
    // });  // 不理解的话可以看一下wait的源码
    // // 再次优化 用户任务阻塞不能超过1s
    notFull_.wait_for(lock, std::chrono::seconds(1), [&] () {
        return taskQue_.size() < TASK_MAX_THRESHOLD;
    });

    // 有空余 将任务添加到任务队列
    taskQue_.emplace(sp);
    ++taskSize_;

    // 通知有任务
    notEmpty_.notify_all(); // 通知有任务了

}

// 开启线程池
void ThreadPool::start(int initThreadSize)
{
    initThreadSize_ = initThreadSize;

    // 创建线程对象
    for (int i = 0; i < initThreadSize_; ++i)
    {
        // 创建线程对象并绑定线程函数
        
        // threads_.emplace_back(new Thread(std::bind(&ThreadPool::threadFunc, this)));
        // c++14
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this));
        threads_.emplace_back(std::move(ptr));
        // threads_.emplace_back(ptr);  // 这是c++语言层面的问题
    }

    // 启动线程
    for (int i = 0; i < initThreadSize_; ++i)
    {
        // 启动线程的代码
        threads_[i]->start();
    }
}

// 定义线程函数
void ThreadPool::threadFunc()
{
    // 测试
    std::cout << "begin threadFunc tid: " << std::this_thread::get_id() << std::endl;
    std::cout << "end threadfunc tid: " << std::this_thread::get_id() << std::endl;
}

// **************************线程方法实现*****************************
// 构造函数，传入线程函数
Thread::Thread(ThreadFunc func)
    : func_(std::move(func))
{
}

// 析构函数
Thread::~Thread()
{
}

// 启动线程
void Thread::start()
{
    // 创建线程并执行传入的函数
    std::thread t(func_);
    t.detach(); // 分离线程
}