#ifndef AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H
#define AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H

#include <cstring>
#include <memory>
#include <queue>

#include <sys/epoll.h>
#include <afina/Storage.h>
#include <spdlog/logger.h>
#include <afina/execute/Command.h>
#include <protocol/Parser.h>

namespace Afina {
namespace Network {
namespace MTnonblock {

class Connection {
public:
    using MutexGetter = std::unique_lock<std::mutex>;
    Connection(int s, std::shared_ptr<Afina::Storage>& ps, std::shared_ptr<spdlog::logger>& pl) : _socket(s),
    pStorage(ps), _logger(pl), _read_bytes(0),_written(0) {
        _arg_remains = 0;
        std::memset(&_event, 0, sizeof(struct epoll_event));
        _is_alive.store(true);
        _event.data.ptr = this;
    }

    inline bool isAlive() const { return _is_alive; }

    void Start();

protected:
    void OnError();
    void OnClose();
    void DoRead();
    void DoWrite();

private:
    friend class ServerImpl;
    friend class Worker;

    int _socket;
    struct epoll_event _event{};
    std::shared_ptr<Afina::Storage> &pStorage;
    std::shared_ptr<spdlog::logger> &_logger;
    std::atomic<bool> _is_alive;

    size_t _read_bytes;
    size_t _written;
    char _read_buf[4096] = "";

    std::mutex _mutex;

    std::vector<std::string> _queue;
    std::size_t _arg_remains;
    Protocol::Parser _parser;
    std::string _argument_for_command;
    std::unique_ptr<Execute::Command> _command_to_execute;
};

} // namespace MTnonblock
} // namespace Network
} // namespace Afina

#endif // AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H
