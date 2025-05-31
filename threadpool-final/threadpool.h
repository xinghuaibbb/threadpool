#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <vector>
#include <queue>
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <stdexcept>
#include <functional>
#include <unordered_map>
#include <thread>
#include <future>
#include <iostream>

#if 0
// any类型
class Any
{
public:
    Any() = default;
    ~Any() = default;

    // 由于成员变量使用了unique_ptr, 因此对象也提供 禁止拷贝和赋值, 允许右值拷贝和赋值
    // 禁止左值引用拷贝和赋值
    Any(const Any&) = delete;
    Any& operator=(const Any&) = delete;
    // 允许右值引用拷贝和赋值
    Any(Any&&) = default;
    Any& operator=(Any&&) = default;

    // 模板构造函数, 用于存储任意类型数据
    template <typename T>
    Any(T data) : base_(std::make_unique<Derive<T>>(data))
    {}

    // 获取存储的数据的类型
    template <typename T>
    T cast_()
    {
        // 怎么获取存储的数据的类型呢?
        // 使用dynamic_cast将基类指针转换为派生类指针
        // 如果转换失败, 返回nullptr
        // 如果转换成功, 返回派生类指针
        // 注意: 这里的T必须是派生类的类型, 否则会抛出异常
        Derive<T>* pd = dynamic_cast<Derive<T> *>(base_.get());

        if (pd == nullptr)
        {
            throw "type is unmatch"; // 如果转换失败, 抛出异常
        }

        return pd->data_; // 返回存储的数据
    }

private:
    // 基类类型
    class Base
    {
    public:
        virtual ~Base() = default;
    };

    // 派生类类型
    template <typename T>
    class Derive : public Base
    {
    public:
        Derive(T data) : data_(data) {}
        T data_;
    };

    std::unique_ptr<Base> base_; // 存储任意类型数据
};

// 实现信号量
class Semaphore
{
public:
    Semaphore(int limit = 0) : resLimit(limit)
    {
        if (limit < 0)
        {
            throw std::invalid_argument("Semaphore limit must be greater than 0");
        }
    }
    ~Semaphore() = default;

    void wait()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        // 等待直到信号量计数大于0
        cond_.wait(lock, [&]() -> bool
            {
                return resLimit > 0;
            });
        --resLimit; // 减少信号量计数
    }

    void post()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        ++resLimit;         // 增加信号量计数
        cond_.notify_all(); // 通知一个等待的线程
    }

private:
    std::mutex mutex_;             // 互斥锁
    std::condition_variable cond_; // 条件变量
    int resLimit;                  // 信号量计数
};


class Task; // 前向声明Task类
// 定义任务返回值
class Result
{
public:
    Result(std::shared_ptr<Task> task, bool isReady = true);
    ~Result() = default;
    Result(Result&&)
    {
        // 移动构造函数
        isReady_ = true; // 移动后任务已准备好
        task_ = std::move(task_);
    }
    // 这两个要好好思考一下
    // 获取返回值并赋值
    void setValue(Any any);

    // 用户获取任务返回值
    Any get();

private:
    Any any_;                    // 存储任务返回值
    Semaphore sem_;              // 信号量，用于同步任务完成
    std::shared_ptr<Task> task_; // 任务指针
    std::atomic_bool isReady_;   // 任务是否完成标志
};

// 抽象任务基类
class Task
{
public:
    Task();
    ~Task() = default;

    void exec();
    void setResult(Result* res);
    virtual Any run() = 0;

private:
    Result* result_; // 任务执行结果
};

#endif


// 线程池模式
enum class PoolMode
{
    MODE_FIXED,  // 固定线程池
    MODE_CACHED, // 动态变化线程池
};


// 线程类型
class Thread
{
public:
    using ThreadFunc = std::function<void(int)>;

    // 构造函数，传入线程函数
    Thread(ThreadFunc func);

    // 析构函数
    ~Thread();

    // 启动线程
    void start();

    // 获取线程ID
    int getThreadId() const;

private:
    ThreadFunc func_; // 线程函数
    int threadId_; // 保存线程ID
    static int generate_id; // 静态变量，用于生成唯一的线程ID

};

/*
**********************************example**************************
ThreadPool pool;
pool.setMode(PoolMode::MODE_FIXED); // 设置线程池模式
pool.start(4); // 启动线程池，初始线程数为4
class MyTask : public Task {
public:
    void run() override {
        // 重写任务执行逻辑
    }
};

pool.submitTask(std::make_shared<MyTask>()); // 提交任务到线程池

*/

// 线程池类型
class ThreadPool
{
public:
    ThreadPool();
    ~ThreadPool();

    // 设置线程池模式
    void setMode(PoolMode mode);

    // 设置task队列最大线程数
    void setTaskQueMaxThreshHold(int size);   // 不是优化掉, start直接传入, 而是两种情况 都可以

    // 设置线程池线程数量阈值, 用于动态变化线程池模式
    void setThreadSizeThreshHold(int size);


    // 修改 使用可变参模板
    // 提交任务到线程池
    // Result submitTask(std::shared_ptr<Task> sp);

    // 提交任务到线程池---使用可变参模板
    template <typename Func, typename... Args>
    auto submitTask(Func&& func, Args &&...args)
        -> std::future<decltype(func(args...))>
    {
        // 打包任务, 放入任务队列
        using RType = decltype(func(args...)); // 获取函数返回值类型
        auto task = std::make_shared<std::packaged_task<RType()>>(
            std::bind(std::forward<Func>(func), std::forward<Args>
                (args)...)
        );
        std::future<RType> result = task->get_future(); // 获取任务的future对象

        // 获取锁
        std::unique_lock<std::mutex> lock(taskQueMutex_);

        if (!notFull_.wait_for(lock, std::chrono::seconds(1), [&]()->bool
            {
                return taskQue_.size() < (size_t)taskQueMaxThreshHold_;
            }))
        {
            // 超时了, 任务队列满了
            std::cerr << "任务提交失败!!" << std::endl;

            task = std::make_shared<std::packaged_task<RType()>>(
                []()-> RType
                {
                    return RType(); // 返回默认值
                }
            );
            (*task)(); // 执行任务, 返回默认值   --- 这个别忘了
            return task->get_future();
        }

        // 有空余 将任务添加到任务队列
        // taskQue_.emplace(sp);

        // 直接使用 此时的task对象 执行任务
        taskQue_.emplace(
            [task]()
            {
                (*task)();
            }
        );

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


        return result; // 返回结果, 任务提交成功
    }


    // 开启线程池
    void start(int initThreadSize = std::thread::hardware_concurrency());

    // 禁止拷贝和赋值
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

private:
    // 定义线程函数
    void threadFunc(int threadid);

    bool checkPoolState() const;

private:
    // std::vector<std::unique_ptr<Thread>> threads_; // 线程列表
    std::unordered_map<int, std::unique_ptr<Thread>> threads_; // 线程列表, 使用unordered_map存储线程对象
    size_t initThreadSize_;                        // 初始线程数量
    std::atomic_uint idleThreadSize_; // 空闲线程数量-cached需要
    std::atomic_uint ThreadSizeThreshold_; // 线程池线程数量阈值, 用于动态变化线程池模式 cached需要
    std::atomic_uint currentThreadSize_; // 线程池当前线程总数量 cached需要


    //修改task, 不再需要 指针, 因为现在是封装好的, 之前是用户自己创建的Task
    using Task = std::function<void()>;
    std::queue<Task> taskQue_; // 任务队列


    std::atomic_uint taskSize_;                 // 任务数量  线程安全
    int taskQueMaxThreshHold_;                  // 任务队列最大线程数, 阈值

    std::mutex taskQueMutex_;          // 任务队列互斥锁
    std::condition_variable notFull_;  // 任务队列不满
    std::condition_variable notEmpty_; // 任务队列不空
    std::condition_variable exitCond_; // 线程池退出条件变量
    std::condition_variable startCond_; // 线程池全部启动条件变量 -- 复现死锁本人解决办法

    PoolMode poolmode_; // 当前线程池模式
    std::atomic_bool isPoolRunning_; // 线程池是否正在运行


};

#endif