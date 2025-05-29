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

// any类型
class Any
{
public:
    Any() = default;
    ~Any() = default;

    // 由于成员变量使用了unique_ptr, 因此对象也提供 禁止拷贝和赋值, 允许右值拷贝和赋值
    // 禁止左值引用拷贝和赋值
    Any(const Any &) = delete;
    Any &operator=(const Any &) = delete;
    // 允许右值引用拷贝和赋值
    Any(Any &&) = default;
    Any &operator=(Any &&) = default;

    // 模板构造函数, 用于存储任意类型数据
    template <typename T>
    Any(T data) : base_(std::make_unique<Derive<T>>(data))
    {
    }

    // 获取存储的数据的类型
    template <typename T>
    T cast_()
    {
        // 怎么获取存储的数据的类型呢?
        // 使用dynamic_cast将基类指针转换为派生类指针
        // 如果转换失败, 返回nullptr
        // 如果转换成功, 返回派生类指针
        // 注意: 这里的T必须是派生类的类型, 否则会抛出异常
        Derive<T> *pd = dynamic_cast<Derive<T> *>(base_.get());

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
                   { return resLimit > 0; });
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
    Result(Result &&)
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
    Task();
    ~Task();

    void exec();
    void setResult(Result *res);
    virtual Any run() = 0;

private:
    Result *result_; // 任务执行结果
};

// 线程类型
class Thread
{
public:
    using ThreadFunc = std::function<void()>;

    // 构造函数，传入线程函数
    Thread(ThreadFunc func);

    // 析构函数
    ~Thread();

    // 启动线程
    void start();

private:
    ThreadFunc func_; // 线程函数
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

    

    // 提交任务到线程池
    Result submitTask(std::shared_ptr<Task> sp);

    // 开启线程池
    void start(int initThreadSize = 4);

    // 禁止拷贝和赋值
    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;

private:
    // 定义线程函数
    void threadFunc();

    bool checkPoolState() const;

private:
    std::vector<std::unique_ptr<Thread>> threads_; // 线程列表
    size_t initThreadSize_;                        // 初始线程数量
    std::atomic_uint idleThreadSize_; // 空闲线程数量-cached需要
    std::atomic_uint ThreadSizeThreshold_; // 线程池线程数量阈值, 用于动态变化线程池模式 cached需要
    std::atomic_uint currentThreadSize_; // 线程池当前线程总数量 cached需要

    std::queue<std::shared_ptr<Task>> taskQue_; // 任务队列
    std::atomic_uint taskSize_;                 // 任务数量  线程安全
    int taskQueMaxThreshHold_;                  // 任务队列最大线程数, 阈值

    std::mutex taskQueMutex_;          // 任务队列互斥锁
    std::condition_variable notFull_;  // 任务队列不满
    std::condition_variable notEmpty_; // 任务队列不空

    PoolMode poolmode_; // 当前线程池模式
    std::atomic_bool isPoolRunning_; // 线程池是否正在运行

   
};

#endif