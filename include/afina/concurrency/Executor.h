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

    Executor(int low_watermark, int high_watermark, int max_queue_size, int idle_time) : low_watermark(low_watermark),
            high_watermark(high_watermark), max_queue_size(max_queue_size), idle_time(idle_time),
            state(State::kStopped), resting_workers(0), workers(0) {};


    ~Executor() {
        this->Stop(true);
    };

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
     void Start() {
        {
            GetMutex lock(mutex);
            if (state != State::kStopped) {
                throw std::runtime_error("you are trying to start Executor which is already running");
            }
        }
        state = State::kRun;
        for (int i = 0; i < low_watermark; ++i) {
           std::thread new_tr = std::thread(&Executor::perform, this);
           workers++;
           resting_workers++;
           new_tr.detach();
        }
     }

    /**
     * Signal thread pool to stop, it will stop accepting new jobs and close threads just after each become
     * free. All enqueued jobs will be complete.
     *
     * In case if await flag is true, call won't return until all background jobs are done and all threads are stopped
     */
    void Stop(bool await = false) {
        GetMutex lock(mutex);
        if (state == State::kStopped) {
            return;
        }
        state = State::kStopping;
        empty_condition.notify_all();
        if (await) {
            while (state != State::kStopped) {
                wait_threads.wait(lock);
            }
        }
    }

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

        std::unique_lock<std::mutex> lock(this->mutex);
        if (state != State::kRun) {
            return false;
        }

        if (resting_workers == 0 && workers < high_watermark) {
            auto new_tr = std::thread(&Executor::perform, this);
            workers++;
            resting_workers++;
            new_tr.detach();
        }
        if (tasks.size() == max_queue_size) {
            return false;
        }
        // Enqueue new task
        tasks.push_back(exec);
        empty_condition.notify_one();
        return true;
    }


    void perform() {
        while (true) {
            GetMutex lock(mutex);
            if (not tasks.empty() && state == State::kRun) {
                auto task = tasks.front();
                tasks.pop_front();
                resting_workers--;
                lock.unlock();
                task();
                lock.lock();
                resting_workers++;
            } else if (tasks.empty() && state == State::kRun) {
                if (empty_condition.wait_for(lock, std::chrono::milliseconds(idle_time)) ==
                std::cv_status::timeout) {
                    if (workers > low_watermark) {
                        workers--;
                        resting_workers--;
                        return;
                    }
                }
            } else if (not tasks.empty() && state == State::kStopping) {
                auto task = tasks.front();
                tasks.pop_front();
                resting_workers--;
                lock.unlock();
                task();
                lock.lock();
                resting_workers++;
                empty_condition.notify_all();
            } else if (tasks.empty() && state == State::kStopping) {
                workers--;
                resting_workers--;
                empty_condition.notify_all();
                if (workers == 0) {
                    state = State::kStopped;
                    wait_threads.notify_one();
                }
                return;
            }
        }
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

    int low_watermark;

    int high_watermark;

    int max_queue_size;

    int idle_time;

    /**
     * Mutex to protect state below from concurrent modification
     */
    std::mutex mutex;

    /**
     * Conditional variable to await new data in case of empty queue
     */
    std::condition_variable empty_condition;

    std::condition_variable wait_threads;
    /**
     * Vector of actual threads that perorm execution
     */
    std::vector<std::thread> threads;

    /**
     * Task queue
     */
    std::deque<Task> tasks;

    /**
     * Flag to stop bg threads
     */
    State state;

    int resting_workers;

    int workers;

};


} // namespace Concurrency
} // namespace Afina

#endif // AFINA_CONCURRENCY_EXECUTOR_H
