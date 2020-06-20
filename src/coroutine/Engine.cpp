#include <afina/coroutine/Engine.h>

#include <csetjmp>
#include <cstring>
#include <string>

namespace Afina {
namespace Coroutine {
    void Engine::Store(context &ctx) {
        char addr;
        if (&addr <= ctx.Low) {
            ctx.Low = &addr;
        } else {
            ctx.Hight = &addr;
        }

        uint32_t cur_size = ctx.Hight - ctx.Low;
        if (std::get<1>(ctx.Stack) < cur_size) {
            delete[] std::get<0>(ctx.Stack);
            std::get<0>(ctx.Stack) = new char[cur_size];
            std::get<1>(ctx.Stack) = cur_size;
        }
        memcpy(std::get<0>(ctx.Stack), ctx.Low, cur_size);
    }

    void Engine::Restore(context &ctx) {
        char addr;
        if ((&addr <= ctx.Hight) && (&addr >= ctx.Low)) {
            Restore(ctx);
        }

        uint32_t cur_size = ctx.Hight - ctx.Low;
        memcpy(ctx.Low, std::get<0>(ctx.Stack), cur_size);
        cur_routine = &ctx;
        longjmp(ctx.Environment, 1);
    }

    void Engine::yield() {
        if (alive == nullptr || (cur_routine == alive && alive->next == nullptr)) {
            return;
        }
        context *next_routine = alive;
        if (alive != nullptr && alive == cur_routine) {
            next_routine = alive->next;
        }
        if (next_routine != nullptr) {
            if (cur_routine != nullptr && cur_routine != idle_ctx) {
                if (setjmp(cur_routine->Environment) > 0) {
                    return;
                }
                Store(*cur_routine);
            }
            //cur_routine = next_routine;
            Restore(*next_routine);
        }
    }

    void Engine::sched(void *routine_) {
        context* next_routine = static_cast<context *>(routine_);
        if (next_routine == cur_routine || next_routine->is_block) {
            return;
        }
        if (next_routine != nullptr) {
            if (cur_routine != nullptr && cur_routine != idle_ctx) {
                if (setjmp(cur_routine->Environment) > 0) {
                    return;
                }
                Store(*cur_routine);
            }
            //cur_routine = static_cast<context *>(routine_);
            Restore(*next_routine);
        } else {
            yield();
        }
    }

    void Engine::block(void *coro) {
        context *coro_to_block;
        if (coro == nullptr) {
            coro_to_block = cur_routine;
        } else {
            coro_to_block = static_cast<context *>(coro);
        }
        if (coro_to_block == nullptr || coro_to_block->is_block) {
            return;
        }
        if (coro_to_block == alive) {
            alive = alive->next;
        }
        if (coro_to_block->next != nullptr) {
            coro_to_block->next->prev = coro_to_block->prev;
        }
        if (coro_to_block->prev != nullptr) {
            coro_to_block->prev->next = coro_to_block->next;
        }

        coro_to_block->next = blocked;
        coro_to_block->prev = nullptr;
        blocked = coro_to_block;
        if (coro_to_block->next != nullptr) {
            coro_to_block->next->prev = coro_to_block;
        }
        coro_to_block->is_block = true;
        if (cur_routine == coro_to_block) {
            if (coro_to_block != idle_ctx && coro_to_block != nullptr) {
                if (setjmp(coro_to_block->Environment) > 0) {
                    return;
                } else {
                    Store(*coro_to_block);
                }
            }
            Restore(*idle_ctx);
        }
    }

    void Engine::unblock(void *coro) {
        auto *coro_to_unblock = static_cast<context *>(coro);
        if (coro_to_unblock == nullptr || !coro_to_unblock->is_block) {
            return;
        }
        if (blocked == coro_to_unblock) {
            blocked = blocked->next;
        }
        if (coro_to_unblock->next != nullptr) {
            coro_to_unblock->next->prev = coro_to_unblock->prev;
        }
        if (coro_to_unblock->prev != nullptr) {
            coro_to_unblock->prev->next = coro_to_unblock->next;
        }

        coro_to_unblock->next = alive;
        coro_to_unblock->prev = nullptr;
        alive = coro_to_unblock;
        if (coro_to_unblock->next != nullptr) {
            coro_to_unblock->next->prev = coro_to_unblock;
        }
        coro_to_unblock->is_block = false;
    }

    void Engine::all_unblock() {
        context *coro = blocked;
        while (coro != nullptr) {
            unblock(coro);
            coro = coro->next;
        }
    }

    Engine::context *Engine::get_curroutine() { return cur_routine; }
} // namespace Coroutine
} // namespace Afina