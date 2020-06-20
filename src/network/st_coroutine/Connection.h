#pragma once

#include <cstring>

#include <afina/Storage.h>
#include <afina/coroutine/Engine.h>
#include <protocol/Parser.h>
#include <spdlog/logger.h>
#include <sys/epoll.h>

namespace Afina {
namespace Network {
namespace STcoroutine {

    class Connection {
    public:
        Connection(int s, std::shared_ptr<Afina::Storage> &ps, std::shared_ptr<spdlog::logger> &pl,
                   Afina::Coroutine::Engine *engine)
                : _socket(s), _pStorage(ps), _logger(pl), _engine(engine) {
            is_alive = false;
            _ctx = nullptr;
        }

        inline bool isAlive() const { return is_alive; }

        void Start();

    protected:
        void OnError();
        void OnClose();
        void Process();

    private:
        friend class ServerImpl;

        int _socket;
        struct epoll_event _event;
        bool is_alive;
        Afina::Coroutine::Engine::context *_ctx;
        Afina::Coroutine::Engine *_engine;

        std::shared_ptr<Afina::Storage> _pStorage;
        std::shared_ptr<spdlog::logger> _logger;
    };

} // namespace STcoroutine
} // namespace Network
} // namespace Afina
