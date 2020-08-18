//
// Created by iffi on 20-1-13.
//

#ifndef THREAD_POOL_H
#define THREAD_POOL_H


#include <concurrentqueue/blockingconcurrentqueue.h>

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>
#include <type_traits>

#define THREAD_POOL_SLEEP_USEC  100

struct void_ctx {
};

/**
 * @class ThreadPoolConcurrent
 * @brief A thread pool implemenatation using lockless concurrent queue
 * @note May not support NUMA architectures well
 */
template<typename Context = void_ctx>
class ThreadPoolConcurrent {
public:
    explicit ThreadPoolConcurrent(size_t size);

    /**
     * @tparam F
     * @tparam Args
     * @param f     Function to be called.
     * @param args  Function arguments.
     * @return      Function result.
     */
    template<typename CTX=Context,
            typename std::enable_if<not std::is_same<CTX, void_ctx>::value, int>::type = 0,
            class F, class... Args>
    auto enqueue(F &&f, Args &&... args)
    -> std::future<typename std::result_of<F(Context & , Args...)>::type>;

    template<typename CTX=Context,
            typename std::enable_if<std::is_same<CTX, void_ctx>::value, int>::type = 0,
            class F, class... Args>
    auto enqueue(F &&f, Args &&... args)
    -> std::future<typename std::result_of<F(Args...)>::type>;

    /**
     * Resize the thread pool.
     * @param size New thread pool size.
     */
    void resize(size_t size);

    template<typename CTX = Context,
            typename std::enable_if<not std::is_same<CTX, void_ctx>::value, int>::type = 0>
    Context &getContext(size_t idx);

    template<typename CTX = Context,
            typename std::enable_if<not std::is_same<CTX, void_ctx>::value, int>::type = 0,
            typename Ctx>
    void setContext(size_t idx, Ctx &&context);

    /**
     * Get size of the thread pool.
     * @return Thread pool size.
     */
    size_t size();

    /**
     * Get size of the task queue.
     * @return Task queue size.
     */
    size_t qsize();

    ~ThreadPoolConcurrent();

private:
    std::vector<Context> _worker_ctx;
    std::vector<std::thread> _workers;
    // the task queue
    moodycamel::BlockingConcurrentQueue<std::function<void(Context &)>> _tasks;
    bool _stop;
    size_t _size;
    std::mutex _resize_lock;
};

template<typename Context>
ThreadPoolConcurrent<Context>::ThreadPoolConcurrent(size_t size)
        : _stop(false), _size(0) {
    resize(size);
}

template<typename Context>
template<typename CTX,
        typename std::enable_if<not std::is_same<CTX, void_ctx>::value, int>::type,
        class F, class... Args>
auto ThreadPoolConcurrent<Context>::enqueue(F &&f, Args &&... args)
-> std::future<typename std::result_of<F(Context & , Args...)>::type> {
    using return_type = typename std::result_of<F(Context & , Args...)>::type;

    auto task = std::make_shared<std::packaged_task<return_type(Context &)> >(
            std::bind(std::forward<F>(f), std::placeholders::_1, std::forward<Args>(args)...)
    );

    std::future<return_type> res = task->get_future();
    if (_stop)
        throw std::runtime_error("Enqueue on stopped ThreadPool");
    this->_tasks.enqueue([task](Context &c) { (*task)(c); });
    return res;
}

template<typename Context>
template<typename CTX,
        typename std::enable_if<std::is_same<CTX, void_ctx>::value, int>::type,
        class F, class... Args>
auto ThreadPoolConcurrent<Context>::enqueue(F &&f, Args &&... args)
-> std::future<typename std::result_of<F(Args...)>::type> {
    using return_type = typename std::result_of<F(Args...)>::type;

    /// std::bind will ignore the extraneous "Context" parameter
    auto task = std::make_shared<std::packaged_task<return_type()> >(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<return_type> res = task->get_future();
    if (_stop)
        throw std::runtime_error("Enqueue on stopped ThreadPool");
    this->_tasks.enqueue([task](Context &c) { (*task)(); });
    return res;
}

template<typename Context>
void ThreadPoolConcurrent<Context>::resize(size_t size) {
    _resize_lock.lock();
    if (_size > size) {
        _size = size;
        size_t count = 0;
        /// if thread num is larger than target size, wait for extra threads to stop
        for (std::thread &worker : _workers) {
            if (count++ >= size) {
                worker.join();
            }
        }
        _worker_ctx.resize(size);
    }
    else {
        _worker_ctx.resize(size);
        size_t from = _size;
        _size = size;
        for (size_t i = from; i < size; ++i) {
            _workers.emplace_back(
                    [this, i] {
                        std::function<void(Context &)> task;
                        while (not this->_stop and i < this->_size) {
                            if (this->_tasks.wait_dequeue_timed(
                                    task, std::chrono::milliseconds(THREAD_POOL_SLEEP_USEC))) {
                                task(_worker_ctx[i]);
                            }
                        }
                    }
            );
        }
    }
    _resize_lock.unlock();
}

template<typename Context>
template<typename CTX,
        typename std::enable_if<not std::is_same<CTX, void_ctx>::value, int>::type>
Context &ThreadPoolConcurrent<Context>::getContext(size_t idx) {
    if (idx >= _worker_ctx.size())
        throw std::runtime_error("Invalid index for worker context.");
    return _worker_ctx[idx];
}

template<typename Context>
template<typename CTX,
        typename std::enable_if<not std::is_same<CTX, void_ctx>::value, int>::type,
        typename Ctx>
void ThreadPoolConcurrent<Context>::setContext(size_t idx, Ctx &&context) {
    if (idx >= _worker_ctx.size())
        throw std::runtime_error("Invalid index for worker context.");
    _worker_ctx[idx] = context;
}

template<typename Context>
inline size_t ThreadPoolConcurrent<Context>::size() {
    return _size;
}

template<typename Context>
inline size_t ThreadPoolConcurrent<Context>::qsize() {
    return _tasks.size_approx();
}

template<typename Context>
inline ThreadPoolConcurrent<Context>::~ThreadPoolConcurrent() {
    _stop = true;
    for (std::thread &worker: _workers)
        worker.join();
}

/**
 * @class ThreadPoolUsingLock
 * @brief A thread pool implemenatation using std::mutex and std::conditional_variable
 */
template<typename Context = void_ctx>
class ThreadPoolUsingLock {
public:
    explicit ThreadPoolUsingLock(size_t size);

    /**
     * @tparam F
     * @tparam Args
     * @param f     Function to be called.
     * @param args  Function arguments.
     * @return      Function result.
     */
    template<typename CTX=Context,
            typename std::enable_if<not std::is_same<CTX, void_ctx>::value, int>::type = 0,
            class F, class... Args>
    auto enqueue(F &&f, Args &&... args)
    -> std::future<typename std::result_of<F(Context & , Args...)>::type>;

    template<typename CTX=Context,
            typename std::enable_if<std::is_same<CTX, void_ctx>::value, int>::type = 0,
            class F, class... Args>
    auto enqueue(F &&f, Args &&... args)
    -> std::future<typename std::result_of<F(Args...)>::type>;

    /**
     * Resize the thread pool.
     * @param size New thread pool size.
     */
    void resize(size_t size);

    template<typename CTX = Context,
            typename std::enable_if<not std::is_same<CTX, void_ctx>::value, int>::type = 0>
    Context &getContext(size_t idx);

    template<typename CTX = Context,
            typename std::enable_if<not std::is_same<CTX, void_ctx>::value, int>::type = 0,
            typename Ctx>
    void setContext(size_t idx, Ctx &&context);

    /**
     * Get size of the thread pool.
     * @return Thread pool size.
     */
    size_t size();

    /**
     * Get size of the task queue.
     * @return Task queue size.
     */
    size_t qsize();

    ~ThreadPoolUsingLock();

private:
    std::vector<Context> _worker_ctx;
    std::vector<std::thread> _workers;
    std::queue<std::function<void(Context &)> > _tasks;
    std::mutex _queue_mutex;
    std::condition_variable _condition;
    bool _stop;
    size_t _size;
    std::mutex _resize_lock;
};

template<typename Context>
ThreadPoolUsingLock<Context>::ThreadPoolUsingLock(size_t size)
        : _stop(false), _size(0) {
    resize(size);
}

template<typename Context>
template<typename CTX,
        typename std::enable_if<not std::is_same<CTX, void_ctx>::value, int>::type,
        class F, class... Args>
auto ThreadPoolUsingLock<Context>::enqueue(F &&f, Args &&... args)
-> std::future<typename std::result_of<F(Context & , Args...)>::type> {
    using return_type = typename std::result_of<F(Context & , Args...)>::type;

    auto task = std::make_shared<std::packaged_task<return_type(Context &)> >(
            std::bind(std::forward<F>(f), std::placeholders::_1, std::forward<Args>(args)...)
    );

    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(_queue_mutex);

        // don't allow enqueueing after stopping the pool
        if (_stop)
            throw std::runtime_error("Enqueue on stopped ThreadPool");

        _tasks.emplace([task](Context &c) { (*task)(c); });
    }
    _condition.notify_one();
    return res;
}

template<typename Context>
template<typename CTX,
        typename std::enable_if<std::is_same<CTX, void_ctx>::value, int>::type,
        class F, class... Args>
auto ThreadPoolUsingLock<Context>::enqueue(F &&f, Args &&... args)
-> std::future<typename std::result_of<F(Args...)>::type> {
    using return_type = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared<std::packaged_task<return_type()> >(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(_queue_mutex);

        // don't allow enqueueing after stopping the pool
        if (_stop)
            throw std::runtime_error("Enqueue on stopped ThreadPool");

        _tasks.emplace([task](Context &c) { (*task)(); });
    }
    _condition.notify_one();
    return res;
}

template<typename Context>
inline void ThreadPoolUsingLock<Context>::resize(size_t size) {
    _resize_lock.lock();
    if (_size > size) {
        _size = size;
        size_t count = 0;
        /// if thread num is larger than target size, wait for extra threads to stop
        for (std::thread &worker : _workers) {
            if (count++ >= size) {
                worker.join();
            }
        }
        _worker_ctx.resize(size);
    }
    else {
        _worker_ctx.resize(size);
        size_t from = _size;
        _size = size;
        for (size_t i = from; i < size; ++i) {
            _workers.emplace_back(
                    [this, i] {
                        for (;;) {
                            std::function<void(Context &)> task;
                            {
                                std::unique_lock<std::mutex> lock(this->_queue_mutex);
                                while (not this->_condition.wait_for(lock, std::chrono::microseconds(THREAD_POOL_SLEEP_USEC),
                                                                     [this] { return this->_stop || !this->_tasks.empty(); }))
                                    continue;
                                if (this->_stop || this->_size <= i)
                                    return;
                                if (this->_tasks.size() > 0) {
                                    task = std::move(this->_tasks.front());
                                    this->_tasks.pop();
                                }
                            }
                            task(_worker_ctx[i]);
                        }
                    }
            );
        }
    }
    _resize_lock.unlock();
}

template<typename Context>
template<typename CTX,
        typename std::enable_if<not std::is_same<CTX, void_ctx>::value, int>::type>
Context &ThreadPoolUsingLock<Context>::getContext(size_t idx) {
    if (idx >= _worker_ctx.size())
        throw std::runtime_error("Invalid index for worker context.");
    return _worker_ctx[idx];
}

template<typename Context>
template<typename CTX,
        typename std::enable_if<not std::is_same<CTX, void_ctx>::value, int>::type,
        typename Ctx>
inline void ThreadPoolUsingLock<Context>::setContext(size_t idx, Ctx &&context) {
    if (idx >= _worker_ctx.size())
        throw std::runtime_error("Invalid index for worker context.");
    _worker_ctx[idx] = context;
}

template<typename Context>
inline size_t ThreadPoolUsingLock<Context>::size() {
    return _size;
}

template<typename Context>
inline size_t ThreadPoolUsingLock<Context>::qsize() {
    _queue_mutex.lock();
    size_t qsize = _tasks.size();
    _queue_mutex.unlock();
    return qsize;
}

template<typename Context>
inline ThreadPoolUsingLock<Context>::~ThreadPoolUsingLock() {
    {
        std::unique_lock<std::mutex> lock(_queue_mutex);
        _stop = true;
    }
    _condition.notify_all();
    for (std::thread &worker: _workers)
        worker.join();
}

typedef ThreadPoolConcurrent<> TPCNoCtx;
typedef ThreadPoolUsingLock<> TPLNoCtx;

#endif //THREAD_POOL_H
