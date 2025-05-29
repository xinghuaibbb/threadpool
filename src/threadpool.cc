#include "threadpool.h"
#include <functional>
#include <iostream>
#include <thread>

const int TASK_MAX_THRESHOLD = 4;    // 任务队列最大阈值
const int Thread_MAX_THRESHOLD = 10; // 线程池最大线程数阈值

ThreadPool::ThreadPool()
    : initThreadSize_(4)
    , taskQueMaxThreshHold_(TASK_MAX_THRESHOLD)
    , ThreadSizeThreshold_(Thread_MAX_THRESHOLD)
    , idleThreadSize_(0)
    , currentThreadSize_(0)
    , taskSize_(0)
    , poolmode_(PoolMode::MODE_FIXED)
    , isPoolRunning_(false)
{
    // 初始化线程池
}

ThreadPool::~ThreadPool()
{
    // 没有new过, 不用写
}

// 检查线程池状态
bool ThreadPool::checkPoolState() const
{
    // 检查线程池是否正在运行
    return isPoolRunning_;
}

// 设置线程池模式--手动设置
void ThreadPool::setMode(PoolMode mode)
{
    if (checkPoolState() != true) // 线程池未运行
    {
        poolmode_ = mode; // 设置线程池模式
    }
    else
    {
        std::cerr << "线程池已经在运行, 无法修改模式!" << std::endl;
        return;
    }
}

// 设置task队列最大线程数
void ThreadPool::setTaskQueMaxThreshHold(int size)
{
    if (checkPoolState() == true)
    {
        std::cerr << "线程池已经在运行, 无法修改任务队列最大线程数!" << std::endl;
        return;
    }
    taskQueMaxThreshHold_ = size; // 设置任务队列最大线程数
}

// 设置线程池cached模式线程数量阈值, 用于动态变化线程池模式
void ThreadPool::setThreadSizeThreshHold(int size)
{
    if (checkPoolState() == true)
    {
        std::cerr << "线程池已经在运行, 无法修改线程池线程数量阈值!" << std::endl;
        return;
    }
    if (poolmode_ == PoolMode::MODE_CACHED)
    {
        ThreadSizeThreshold_ = size; // 设置线程池线程数量阈值
    }
    else
    {
        std::cerr << "线程池模式不是动态变化线程池, 无法修改线程池线程数量阈值!"
            << std::endl;
        return;
    }
}

// 提交任务到线程池
Result ThreadPool::submitTask(std::shared_ptr<Task> sp)
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

    if (!notFull_.wait_for(lock, std::chrono::seconds(1), [&]()
        { return taskQue_.size() < (size_t)TASK_MAX_THRESHOLD; }))
    {
        // 超时了, 任务队列满了
        std::cerr << "任务提交失败!!" << std::endl;

        // return; // 任务提交失败
        // return task->getResult(); // 设计细节
        return Result(sp, false);
        // Result res(sp, false);
        // return std::move(res); // 返回结果, 任务提交失败
    }

    // 有空余 将任务添加到任务队列
    taskQue_.emplace(sp);
    ++taskSize_;

    // 通知有任务
    notEmpty_.notify_all(); // 通知有任务了

    if (poolmode_ == PoolMode::MODE_CACHED && taskSize_ > idleThreadSize_ && currentThreadSize_ < ThreadSizeThreshold_)
    {
        auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this));
        threads_.emplace_back(std::move(ptr)); // 创建新的线程
    }

    return Result(sp, false);
}

// 开启线程池
void ThreadPool::start(int initThreadSize)
{
    initThreadSize_ = initThreadSize;
    currentThreadSize_ = initThreadSize;

    // 创建线程对象
    for (int i = 0; i < initThreadSize_; ++i)
    {
        // 创建线程对象并绑定线程函数

        // threads_.emplace_back(new Thread(std::bind(&ThreadPool::threadFunc,
        // this))); c++14
        auto ptr =
            std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this));
        threads_.emplace_back(std::move(ptr));
        // threads_.emplace_back(ptr);  // 这是c++语言层面的问题
    }

    // 启动线程
    for (int i = 0; i < initThreadSize_; ++i)
    {
        // 启动线程的代码
        threads_[i]->start();

        idleThreadSize_++; // 空闲线程数量加1
    }
}

// 定义线程函数
void ThreadPool::threadFunc()
{
    // // 测试
    // std::cout << "begin threadFunc tid: " << std::this_thread::get_id() <<
    // std::endl; std::cout << "end threadfunc tid: " <<
    // std::this_thread::get_id() << std::endl;
    this->isPoolRunning_ = true; // 线程池开始运行
    for (;;)
    {
        // 获取锁
        std::unique_lock<std::mutex> lock(taskQueMutex_);

        std::cout << "Thread " << std::this_thread::get_id() << "尝试获取任务..."
            << std::endl;

        // 等待任务队列不空
        notEmpty_.wait(lock, [&]() -> bool
            { return !taskQue_.empty(); });

        std::cout << "Thread " << std::this_thread::get_id()
            << "获取到任务, 开始执行..." << std::endl;

        idleThreadSize_--; // 空闲线程数量减1

        // 取出任务
        auto task = taskQue_.front();
        taskQue_.pop();
        --taskSize_;

        // 如果还有任务, 通知其他的线程执行
        if (taskSize_ > 0)
        {
            notEmpty_.notify_all(); // 通知其他线程有任务了
        }

        // 通知任务队列不满
        notFull_.notify_all();

        // 解锁
        lock.unlock();

        // 执行任务
        if (task != nullptr)
        {
            // task->run(); // 执行任务
            task->exec(); // 执行任务
        }

        idleThreadSize_++; // 空闲线程数量加1
    }
}

// **************************task实现*****************************
Task::Task()
    : result_(nullptr) // 初始化任务执行结果为nullptr
{
}
Task::~Task()
{
    // 任务析构函数
    // 如果有结果, 释放结果
    if (result_ != nullptr)
    {
        delete result_;
        result_ = nullptr;
    }
}

void Task::exec()
{
    // run();
    if (result_ != nullptr)
    {

        // 设置任务执行结果
        result_->setValue(run());
    }
    else
    {
        std::cerr << "Task result is not set!" << std::endl;
    }
}
void Task::setResult(Result* res)
{
    // 设置任务执行结果
    result_ = res;
}

// **************************线程方法实现*****************************
// 构造函数，传入线程函数
Thread::Thread(ThreadFunc func) : func_(std::move(func)) {}

// 析构函数
Thread::~Thread() {}

// 启动线程
void Thread::start()
{
    // 创建线程并执行传入的函数
    std::thread t(func_);
    t.detach(); // 分离线程
}

// **************************Result实现*****************************
Result::Result(std::shared_ptr<Task> task, bool isReady)
    : task_(task), isReady_(isReady)
{
    task_->setResult(this); // 将本对象作为任务的成员, 用于接收返回值
}

Any Result::get()
{
    // 等待任务完成
    sem_.wait();

    // 返回任务结果
    return std::move(any_);
}

void Result::setValue(Any any)
{
    // 存储task返回值
    this->any_ = std::move(any);
    sem_.post(); // 任务完成, 通知等待的线程
}
