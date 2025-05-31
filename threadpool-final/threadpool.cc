#include "threadpool.h"
#include <functional>
#include <iostream>
#include <thread>

const int TASK_MAX_THRESHOLD = INT32_MAX;    // 任务队列最大阈值
const int Thread_MAX_THRESHOLD = 10; // 线程池最大线程数阈值
const int THREAD_TIMEOUT = 10; // 线程空闲时间超过60s, 则回收多余的线程

ThreadPool::ThreadPool()
    : taskQueMaxThreshHold_(TASK_MAX_THRESHOLD),
    ThreadSizeThreshold_(Thread_MAX_THRESHOLD), idleThreadSize_(0),
    currentThreadSize_(0), taskSize_(0), poolmode_(PoolMode::MODE_FIXED),
    isPoolRunning_(false)
{
    // 初始化线程池
}

// 线程池析构函数
ThreadPool::~ThreadPool()
{
    /*-- 复现死锁本人解决办法
    std::cout << "线程池析构函数 调用..." << std::endl;
    std::unique_lock<std::mutex> lock(taskQueMutex_);
    startCond_.wait(lock, [&]() -> bool
        {
            return taskQue_.size() == 0;
        });
    lock.unlock();
    */

    // 睡一秒
    // std::this_thread::sleep_for(std::chrono::seconds(1)); // 睡一秒, 等待线程池全部启动

    std::cout << "线程池析构函数被调用, 正在关闭线程池..." << std::endl;

    isPoolRunning_ = false; // 设置线程池不在运行状态
    // 等待所有线程结束--线程通信
    // 阻塞 & 任务执行中
    std::unique_lock<std::mutex> lock(taskQueMutex_);

    /*-- 复现死锁本人解决办法
    lock.lock(); // 获取锁
    */


    // 所有线程完成任务了, 此时都在等待 状态, 先唤醒
    notEmpty_.notify_all(); // 通知所有线程有任务了
    std::cout << "唤醒所有线程, 准备析构线程池..." << std::endl;
    exitCond_.wait(lock, [&]() -> bool
        {
            return threads_.size() == 0;
        }); // 等待所有线程回收
    std::cout << "线程池已关闭, 所有线程已回收!" << std::endl;

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

    if (!notFull_.wait_for(lock, std::chrono::seconds(1), [&]()->bool
        {
            return taskQue_.size() < (size_t)taskQueMaxThreshHold_;
        }))
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

    if (poolmode_ == PoolMode::MODE_CACHED && taskSize_ > idleThreadSize_ &&
        currentThreadSize_ < ThreadSizeThreshold_)
    {
        std::cout << "创建新线程..." << std::endl;
        // 这里不能使用 线程id,  这是主线程, 打印的都是一样的

        auto ptr =
            std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
        int threadId = ptr->getThreadId(); // 获取线程ID
        threads_.emplace(threadId, std::move(ptr)); // 使用unordered_map存储线程对象

        threads_[threadId]->start(); // 启动线程


        // 修改线程数量相关
        currentThreadSize_++; // 线程池当前线程总数量加1
        idleThreadSize_++; // 空闲线程数量加1
    }


    return Result(sp, true); // 返回结果, 任务提交成功
}

// 开启线程池
void ThreadPool::start(int initThreadSize)
{
    this->isPoolRunning_ = true; // 线程池开始运行
    // {
    //     std::unique_lock<std::mutex> lock(taskQueMutex_);
    this->initThreadSize_ = initThreadSize;
    this->currentThreadSize_ = initThreadSize;

    std::cout << initThreadSize_ << "个线程被创建, 线程池开始运行..." << std::endl;

    // 创建线程对象
    for (int i = 0; i < initThreadSize_; ++i)
    {
        // 创建线程对象并绑定线程函数

        // threads_.emplace_back(new Thread(std::bind(&ThreadPool::threadFunc,
        // this))); c++14
        auto ptr =
            std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));

        int threadId = ptr->getThreadId(); // 获取线程ID

        // threads_.emplace_back(std::move(ptr));
        // threads_.emplace_back(ptr);  // 这是c++语言层面的问题
        threads_.emplace(threadId, std::move(ptr)); // 使用unordered_map存储线程对象
    }

    // 启动线程
    for (int i = 0; i < initThreadSize_; ++i)
    {
        // 启动线程的代码
        threads_[i]->start();

        idleThreadSize_++; // 空闲线程数量加1
    }
    // }

    // startCond_.notify_all(); // 通知线程池全部启动条件变量

}


// 线程池退出, 有任务也得先执行完
// 定义线程函数
void ThreadPool::threadFunc(int threadid)
{

    auto lastTime = std::chrono::high_resolution_clock::now(); // 记录线程开始时间

    // for (;;)
    for (;;)
    {
        std::shared_ptr<Task> task;
        {
            // 获取锁
            std::unique_lock<std::mutex> lock(taskQueMutex_);

            std::cout << "Thread " << std::this_thread::get_id() << "尝试获取任务..."
                << std::endl;

            // 区分超时返回和 任务执行返回
            // 1s返回一次
            while (taskQue_.size() == 0)
            {
                if (!this->isPoolRunning_)
                {
                    threads_.erase(threadid);
                    std::cout << "Thread " << std::this_thread::get_id()
                        << "线程池不在运行状态, 回收线程..." << std::endl;
                    exitCond_.notify_all(); // 通知线程池退出条件变量
                    return;
                }
                // cached模式下, 空闲时间超过60s, 则回收多余的线程
                if (poolmode_ == PoolMode::MODE_CACHED)
                {
                    // 条件变量, 超时返回了
                    if (notEmpty_.wait_for(lock, std::chrono::seconds(1), [&]()-> bool
                        {
                            return taskQue_.size() == 0;
                        }))
                    {
                        auto now = std::chrono::high_resolution_clock::now();
                        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
                        if (duration.count() >= THREAD_TIMEOUT && currentThreadSize_ > initThreadSize_)
                        {
                            // 回收线程
                            // 记录线程数量相关的 需要修改
                            // 把当前线程从线程列表删除--难点: 没有办法匹配 改线程函数 对应哪个 线程对象
                            std::cout << "动态创建的线程, 空闲时间超过10s, 回收线程..." << std::endl;
                            threads_.erase(threadid); // 删除线程对象
                            // 不要使用 std::this_thread::get_id() 

                            idleThreadSize_--; // 空闲线程数量减1
                            currentThreadSize_--; // 线程池当前线程总数量减1


                            exitCond_.notify_all(); // 通知线程池退出条件变量
                            return; // 退出线程函数

                        }
                    }

                }
                else   //fixed模式
                {
                    // 等待任务队列不空
                    notEmpty_.wait(lock);
                }

                // 析构时, 唤醒后, 还是会先走这里
                // 如果线程池不在运行状态, 则退出线程函数
                // 析构情况1 : 原本就是等待
                // 死锁优化
                // if (!isPoolRunning_)
                // {
                //     threads_.erase(threadid); // 删除线程对象
                //     std::cout << "Thread " << std::this_thread::get_id()
                //         << "线程池不在运行状态, 回收线程..." << std::endl;
                //     exitCond_.notify_all(); // 通知线程池退出条件变量
                //     return;
                // }


            }


            std::cout << "Thread " << std::this_thread::get_id()
                << "获取到任务, 开始执行..." << std::endl;

            idleThreadSize_--; // 空闲线程数量减1

            // 取出任务
            task = taskQue_.front();
            taskQue_.pop();
            --taskSize_;

            // -- 复现死锁本人解决办法
            // startCond_.notify_all();

            // 如果还有任务, 通知其他的线程执行
            if (taskQue_.size() > 0)
            {
                notEmpty_.notify_all(); // 通知其他线程有任务了
            }

            // 通知任务队列不满
            notFull_.notify_all();

            // 解锁
            // lock.unlock();
        }
        // 执行任务
        if (task != nullptr)
        {
            // task->run(); // 执行任务
            task->exec(); // 执行任务
        }

        idleThreadSize_++; // 空闲线程数量加1

        // 位置要注意, 这记录的是  任务执行完的时间
        lastTime = std::chrono::high_resolution_clock::now(); // 更新线程开始时间
    }



}

#if 0
// ****************************线程池退出, 有任务 不执行*****************************
// 定义线程函数
void ThreadPool::threadFunc(int threadid)
{

    auto lastTime = std::chrono::high_resolution_clock::now(); // 记录线程开始时间

    this->isPoolRunning_ = true; // 线程池开始运行
    // for (;;)
    while (this->isPoolRunning_) // 线程池运行状态
    {
        // 获取锁
        std::unique_lock<std::mutex> lock(taskQueMutex_);

        std::cout << "Thread " << std::this_thread::get_id() << "尝试获取任务..."
            << std::endl;


        // 区分超时返回和 任务执行返回
        // 1s返回一次
        while (this->isPoolRunning_ && taskQue_.size() == 0)
        {
            // cached模式下, 空闲时间超过60s, 则回收多余的线程
            if (poolmode_ == PoolMode::MODE_CACHED)
            {
                // 条件变量, 超时返回了
                if (std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(1)))
                {
                    auto now = std::chrono::high_resolution_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
                    if (duration.count() >= THREAD_TIMEOUT && currentThreadSize_ > initThreadSize_)
                    {
                        // 回收线程
                        // 记录线程数量相关的 需要修改
                        // 把当前线程从线程列表删除--难点: 没有办法匹配 改线程函数 对应哪个 线程对象
                        std::cout << "动态创建的线程, 空闲时间超过10s, 回收线程..." << std::endl;
                        threads_.erase(threadid); // 删除线程对象
                        // 不要使用 std::this_thread::get_id() 

                        idleThreadSize_--; // 空闲线程数量减1
                        currentThreadSize_--; // 线程池当前线程总数量减1

                        std::cout << "Thread " << std::this_thread::get_id()
                            << "空闲时间超过10s, 回收线程..." << std::endl;
                        return; // 退出线程函数

                    }
                }

            }
            else   //fixed模式
            {
                // 等待任务队列不空
                notEmpty_.wait(lock);
            }

            // 析构时, 唤醒后, 还是会先走这里
            // 如果线程池不在运行状态, 则退出线程函数
            // 析构情况1 : 原本就是等待
            // 死锁优化
            // if (!isPoolRunning_)
            // {
            //     threads_.erase(threadid); // 删除线程对象
            //     std::cout << "Thread " << std::this_thread::get_id()
            //         << "线程池不在运行状态, 回收线程..." << std::endl;
            //     exitCond_.notify_all(); // 通知线程池退出条件变量
            //     return;
            // }


        }
        // 死锁优化
        if (!isPoolRunning_)
        {
            break;
        }

        std::cout << "Thread " << std::this_thread::get_id()
            << "获取到任务, 开始执行..." << std::endl;

        idleThreadSize_--; // 空闲线程数量减1

        // 取出任务
        auto task = taskQue_.front();
        taskQue_.pop();
        --taskSize_;

        // -- 复现死锁本人解决办法
        startCond_.notify_all();

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

        // 位置要注意, 这记录的是  任务执行完的时间
        lastTime = std::chrono::high_resolution_clock::now(); // 更新线程开始时间
    }

    // 删除线程对象 
    // 析构情况2: 任务运行中时
    threads_.erase(threadid);
    std::cout << "Thread " << std::this_thread::get_id()
        << "线程池不在运行状态, 回收线程..." << std::endl;
    exitCond_.notify_all(); // 通知线程池退出条件变量

}
#endif

// **************************task实现*****************************
Task::Task()
    : result_(nullptr) // 初始化任务执行结果为nullptr
{}


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
int Thread::generate_id = 0; // 静态变量, 用于生成唯一的线程ID

Thread::Thread(ThreadFunc func)
    : func_(std::move(func))
    , threadId_(generate_id++) // 生成唯一的线程ID
{}

// 析构函数
Thread::~Thread() {}

// 启动线程
void Thread::start()
{
    // 创建线程并执行传入的函数
    std::thread t(func_, threadId_); // 传入线程ID
    t.detach(); // 分离线程
}

// 获取线程ID
int Thread::getThreadId() const
{
    return threadId_;
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
