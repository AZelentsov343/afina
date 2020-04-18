#ifndef AFINA_CONCURRENCY_EXECUTOR_H
#define AFINA_CONCURRENCY_EXECUTOR_H

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

namespace Afina {
namespace Concurrency {

/**
 * # Thread pool
 */
class Executor {
    using Task = std::function<void ()>;
    using GetMutex = std::unique_lock<std::mutex>;
public:

    Executor(int low_watermark, int high_watermark, int max_queue_size, int idle_time);


    ~Executor();

    // No copy/move/assign allowed
    Executor(const Executor &) = delete;
    Executor(Executor &&) = delete;
    Executor &operator=(const Executor &) = delete;
    Executor &operator=(Executor &&) = delete;

    /**
     * Start thread pool;
     * throws runtime_error if thread pool is already started
     * begins low_watermark threads
     */
     void Start();

    /**
     * Signal thread pool to stop, it will stop accepting new jobs and close threads just after each become
     * free. All enqueued jobs will be complete.
     *
     * In case if await flag is true, call won't return until all background jobs are done and all threads are stopped
     */
    void Stop(bool await = false);

    /**
     * Add function to be executed on the threadpool. Method returns true in case if task has been placed
     * onto execution queue, i.e scheduled for execution and false otherwise.
     *
     * That function doesn't wait for function result. Function could always be written in a way to notify caller about
     * execution finished by itself
     */
    template <typename F, typename... Types> bool Execute(F &&func, Types... args) {
        // Prepare "task"
        Task exec = std::bind(std::forward<F>(func), std::forward<Types>(args)...);

        std::unique_lock<std::mutex> lock(this->_mutex);
        if (_state != State::kRun) {
            return false;
        }

        if (_resting_workers == 0 && _workers < _high_watermark) {
            auto new_tr = std::thread(&Executor::perform, this);
            _workers++;
            _resting_workers++;
            new_tr.detach();
        }
        if (_tasks.size() == _max_queue_size) {
            return false;
        }
        // Enqueue new task
        _tasks.push_back(exec);
        _empty_condition.notify_one();
        return true;
    }

private:
    enum class State {
        // Threadpool is fully operational, tasks could be added and get executed
                kRun,

        // Threadpool is on the way to be shutdown, no ned task could be added, but existing will be
        // completed as requested
                kStopping,

        // Threadppol is stopped
                kStopped
    };
    /**
     * Main function that all pool threads are running. It polls internal task queue and execute tasks
     */
    void perform();

    int _low_watermark;

    int _high_watermark;

    int _max_queue_size;

    int _idle_time;

    /**
     * Mutex to protect state below from concurrent modification
     */
    std::mutex _mutex;

    /**
     * Conditional variable to await new data in case of empty queue
     */
    std::condition_variable _empty_condition;

    std::condition_variable _wait_threads;
    /**
     * Vector of actual threads that perorm execution
     */
    std::vector<std::thread> _threads;

    /**
     * Task queue
     */
    std::deque<Task> _tasks;

    /**
     * Flag to stop bg threads
     */
    State _state;

    int _resting_workers;

    int _workers;

};


} // namespace Concurrency
} // namespace Afina

#endif // AFINA_CONCURRENCY_EXECUTOR_H
