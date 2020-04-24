#include <afina/concurrency/Executor.h>
#include <iostream>

namespace Afina {
namespace Concurrency {

Executor::Executor(int low_watermark, int high_watermark, int max_queue_size, int idle_time) :
        _low_watermark(low_watermark), _high_watermark(high_watermark), _max_queue_size(max_queue_size),
        _idle_time(idle_time), _state(State::kStopped), _resting_workers(0), _workers(0) {}


Executor::~Executor() {
    while (_state != State::kStopped) {
        try {
            this->Stop(true);
        } catch (std::runtime_error& ex) {
            std::cout << "error happened while stopping executor: " << ex.what() <<  "\ntrying again..." << std::endl;
        }
    }
}

void Executor::Start() {

    MutexGetter lock(_mutex);
    if (_state != State::kStopped) {
        throw std::runtime_error("you are trying to start Executor which is already running");
    }

    _state = State::kRun;
    for (int i = 0; i < _low_watermark; ++i) {
        std::thread new_tr = std::thread(&Executor::perform, this);
        _workers++;
        _resting_workers++;
        new_tr.detach();
    }
}

void Executor::Stop(bool await) {
    MutexGetter lock(_mutex);
    if (_state == State::kStopped) {
        return;
    }
    _state = State::kStopping;
    _empty_condition.notify_all();
    if (await) {
        while (_state != State::kStopped) {
            _wait_threads.wait(lock);
        }
    } else if (_workers == 0) {
        _state = State::kStopped;
    }
}

void Executor::perform() {
    while (true) {
        MutexGetter lock(_mutex);
        if (not _tasks.empty() && _state == State::kRun) {
            auto task = _tasks.front();
            _tasks.pop_front();
            _resting_workers--;
            lock.unlock();
            try {
                task();
            } catch (std::runtime_error& ex) {
                std::cout << "error happened while doing task: " << ex.what() << std::endl;
            }
            lock.lock();
            _resting_workers++;
        } else if (_tasks.empty() && _state == State::kRun) {
            auto time_left = std::chrono::milliseconds(_idle_time);
            while (_tasks.empty() && _state == State::kRun) {
                auto prev = std::chrono::system_clock::now();
                if (_empty_condition.wait_for(lock, time_left) == std::cv_status::timeout) {
                    if (_workers > _low_watermark) {
                        _workers--;
                        _resting_workers--;
                        return;
                    }
                } else {
                    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()
                            - prev);
                    time_left -= std::chrono::milliseconds(diff.count());
                }
            }
        } else if (not _tasks.empty() && _state == State::kStopping) {
            auto task = _tasks.front();
            _tasks.pop_front();
            _resting_workers--;
            lock.unlock();
            try {
                task();
            } catch (std::runtime_error& ex) {
                std::cout << "error happened while doing task: " << ex.what() << std::endl;
            }
            lock.lock();
            _resting_workers++;
            _empty_condition.notify_all();
        } else if (_tasks.empty() && _state == State::kStopping) {
            _workers--;
            _resting_workers--;
            if (_workers == 0) {
                _state = State::kStopped;
                _wait_threads.notify_all();
            }
            return;
        }
    }
}



} // namespace Concurrency
} // namespace Afina
