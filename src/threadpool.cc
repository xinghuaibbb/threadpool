#include "threadpool.h"
#include <functional>

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
}

// 开启线程池
void ThreadPool::start(int initThreadSize)
{
    initThreadSize_ = initThreadSize;

    // 创建线程对象
    for(int i = 0; i < initThreadSize_; ++i) 
    {
        // 创建线程对象并绑定线程函数
        threads_.emplace_back(new Thread(std::bind(&ThreadPool::threadFunc, this)));  
    }

    // 启动线程
    for(int i = 0; i < initThreadSize_; ++i) 
    {
        // 启动线程的代码
        threads_[i]->start();
    }
}

// 定义线程函数
void ThreadPool::threadFunc()
{
   
}

// **************************线程方法实现*****************************
// 启动线程
void Thread::start()
{
    // 启动线程的具体实现
    
}