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
namespace STnonblock {

class Connection {
public:
    Connection(int s, std::shared_ptr<Afina::Storage>& ps, std::shared_ptr<spdlog::logger>& pl) : _socket(s),
    pStorage(ps), _logger(pl), read_bytes(0), written(0) {
        arg_remains = 0;
        read_buf = new char[512]();
        std::memset(&_event, 0, sizeof(struct epoll_event));
        is_alive = true;
        _event.data.ptr = this;
    }

    ~Connection() {
        delete[] read_buf;
    }

    inline bool isAlive() const { return is_alive; }

    void Start();

protected:
    void OnError();
    void OnClose();
    void DoRead();
    void DoWrite();

private:
    friend class ServerImpl;

    int _socket;
    struct epoll_event _event;
    std::shared_ptr<Afina::Storage> &pStorage;
    std::shared_ptr<spdlog::logger> &_logger;
    bool is_alive;

    size_t read_bytes;
    size_t written;
    char* read_buf;

    std::vector<std::string> queue;
    std::size_t arg_remains;
    Protocol::Parser parser;
    std::string argument_for_command;
    std::unique_ptr<Execute::Command> command_to_execute;
};

} // namespace STnonblock
} // namespace Network
} // namespace Afina

#endif // AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H
