#include <afina/concurrency/Executor.h>

namespace Afina {
namespace Concurrency {

Executor::Executor(int low_watermark, int high_watermark, int max_queue_size, int idle_time) :
        low_watermark(low_watermark), high_watermark(high_watermark), max_queue_size(max_queue_size),
        idle_time(idle_time), state(State::kStopped), resting_workers(0), workers(0) {}


Executor::~Executor() {
    this->Stop(true);
}

void Executor::Start() {
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

void Executor::Stop(bool await) {
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

void Executor::perform() {
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



} // namespace Concurrency
} // namespace Afina
