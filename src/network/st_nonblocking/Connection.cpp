#include "Connection.h"

#include <iostream>
#include <sys/uio.h>

namespace Afina {
namespace Network {
namespace STnonblock {

// See Connection.h
void Connection::Start() {
    _logger->debug("Connection {} started", _socket);
    _event.data.fd = _socket;
    _event.data.ptr = this;
    _event.events = EPOLLIN | EPOLLHUP | EPOLLERR;
}

// See Connection.h
void Connection::OnError() {
    _logger->debug("Error on {} connection", _socket);
    is_alive = false;
}

// See Connection.h
void Connection::OnClose() {
    _logger->debug("Connection {} closed", _socket);
    is_alive = false;
}

// See Connection.h
void Connection::DoRead() {
    _logger->debug("Do read on {} socket", _socket);

    try {
        int read_count;
        while ((read_count = read(_socket, read_buf + read_bytes, sizeof(read_buf) - read_bytes)) > 0) {
            read_bytes += read_count;
            _logger->debug("Got {} bytes from socket", read_count);

            while (read_bytes > 0) {
                _logger->debug("Process {} bytes", read_bytes);
                // There is no command yet
                if (!command_to_execute) {
                    std::size_t parsed = 0;
                    try {
                        if (parser.Parse(read_buf, read_bytes, parsed)) {
                            // There is no command to be launched, continue to parse input stream
                            // Here we are, current chunk finished some command, process it
                            _logger->debug("Found new command: {} in {} bytes", parser.Name(), parsed);
                            command_to_execute = parser.Build(arg_remains);
                            if (arg_remains > 0) {
                                arg_remains += 2;
                            }
                        }
                    } catch (std::runtime_error &ex) {
                        queue.emplace_back("(?^u:ERROR)");
                        _event.events = EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP;
                        throw std::runtime_error(ex.what());
                    }

                    // Parsed might fails to consume any bytes from input stream. In real life that could happens,
                    // for example, because we are working with UTF-16 chars and only 1 byte left in stream
                    if (parsed == 0) {
                        break;
                    } else {
                        std::memmove(read_buf, read_buf + parsed, read_bytes - parsed);
                        read_bytes -= parsed;
                    }
                }

                // There is command, but we still wait for argument to arrive...
                if (command_to_execute && arg_remains > 0) {
                    _logger->debug("Fill argument: {} bytes of {}", read_bytes, arg_remains);
                    // There is some parsed command, and now we are reading argument
                    std::size_t to_read = std::min(arg_remains, std::size_t(read_bytes));
                    argument_for_command.append(read_buf, to_read);

                    std::memmove(read_buf, read_buf + to_read, read_bytes - to_read);
                    arg_remains -= to_read;
                    read_bytes -= to_read;
                }

                // There is command & argument - RUN!
                if (command_to_execute && arg_remains == 0) {
                    _logger->debug("Start command execution");

                    std::string result;
                    command_to_execute->Execute(*pStorage, argument_for_command, result);

                    // Send response
                    result += "\r\n";

                    queue.emplace_back(result);
                    if (queue.size() < 2) {
                        _event.events = EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP;
                    }

                    // Prepare for the next command
                    command_to_execute.reset();
                    argument_for_command.resize(0);
                    parser.Reset();
                }
            } // while (read_count)
        }
        is_alive = false;
        if (read_bytes == 0) {
            _logger->debug("Connection closed");
        } else {
            throw std::runtime_error(std::string(strerror(errno)));
        }
    } catch (std::runtime_error &ex) {
        _logger->error("Failed to process connection on descriptor {}: {}", _socket, ex.what());
    }
}

// See Connection.h
void Connection::DoWrite() {
    _logger->debug("Writing on {} connection...", _socket);
    auto iov = new iovec[queue.size()]();

    for (size_t i = 0; i < queue.size(); ++i) {
        iov[i].iov_base = &(queue[i][0]);
        iov[i].iov_len = queue[i].size();
    }

    iov[0].iov_len -= written;
    iov[0].iov_base = (char *)(iov[0].iov_base) + written;

    int written_bytes = writev(_socket, iov, queue.size());

    if (written_bytes == -1) {
        OnError();
        return;
    }

    size_t done = 0;
    for (auto& command : queue) {
        if (written_bytes - command.size() < 0) {
            break;
        }
        written_bytes -= command.size();
        done++;
    }

    queue.erase(queue.begin(), queue.begin() + done);
    written = written_bytes;

    if (queue.empty()) {
        _event.events = EPOLLIN | EPOLLERR | EPOLLHUP;
    }
    delete[] iov;
}

} // namespace STnonblock
} // namespace Network
} // namespace Afina
