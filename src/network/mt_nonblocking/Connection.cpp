#include "Connection.h"

#include <iostream>
#include <sys/uio.h>

namespace Afina {
namespace Network {
namespace MTnonblock {

// See Connection.h
void Connection::Start() {
    MutexGetter lock(_mutex);
    _event.data.fd = _socket;
    _event.data.ptr = this;
    _event.events = EPOLLIN | EPOLLHUP | EPOLLERR | EPOLLET;
    _logger->debug("Connection {} started", _socket);
}

// See Connection.h
void Connection::OnError() {
    _is_alive.store(false);
    _logger->debug("Error on {} connection", _socket);
}

// See Connection.h
void Connection::OnClose() {
    _is_alive.store(false);
    _logger->debug("Connection {} closed", _socket);
}

// See Connection.h
void Connection::DoRead() {
    _logger->debug("Reading on {} connection...", _socket);
    MutexGetter lock(_mutex);
    try {
        int bytes;
        while ((bytes = read(_socket, _read_buf + _read_bytes, sizeof(_read_buf) - _read_bytes)) > 0) {
            _read_bytes += bytes;
            _logger->debug("Got {} bytes from socket", bytes);

            while (_read_bytes > 0) {
                _logger->debug("Process {} bytes", _read_bytes);
                // There is no command yet
                if (!_command_to_execute) {
                    std::size_t parsed = 0;
                    try {
                        if (_parser.Parse(_read_buf, _read_bytes, parsed)) {
                            // There is no command to be launched, continue to parse input stream
                            // Here we are, current chunk finished some command, process it
                            _logger->debug("Found new command: {} in {} bytes", _parser.Name(), parsed);
                            _command_to_execute = _parser.Build(_arg_remains);
                            if (_arg_remains > 0) {
                                _arg_remains += 2;
                            }
                        }
                    } catch (std::runtime_error &ex) {
                        _queue.emplace_back("(?^u:ERROR)");
                        _event.events = EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP;
                        throw std::runtime_error(ex.what());
                    }

                    // Parsed might fails to consume any bytes from input stream. In real life that could happens,
                    // for example, because we are working with UTF-16 chars and only 1 byte left in stream
                    if (parsed == 0) {
                        break;
                    } else {
                        std::memmove(_read_buf, _read_buf + parsed, _read_bytes - parsed);
                        _read_bytes -= parsed;
                    }
                }

                // There is command, but we still wait for argument to arrive...
                if (_command_to_execute && _arg_remains > 0) {
                    _logger->debug("Fill argument: {} bytes of {}", _read_bytes, _arg_remains);
                    // There is some parsed command, and now we are reading argument
                    std::size_t to_read = std::min(_arg_remains, std::size_t(_read_bytes));
                    _argument_for_command.append(_read_buf, to_read);

                    std::memmove(_read_buf, _read_buf + to_read, _read_bytes - to_read);
                    _arg_remains -= to_read;
                    _read_bytes -= to_read;
                }

                // There is command & argument - RUN!
                if (_command_to_execute && _arg_remains == 0) {
                    _logger->debug("Start command execution");

                    std::string result;
                    _command_to_execute->Execute(*pStorage, _argument_for_command, result);

                    // Send response
                    result += "\r\n";

                    _queue.emplace_back(result);
                    if (_queue.size() < 2) {
                        _event.events |= EPOLLOUT;
                    }

                    // Prepare for the next command
                    _command_to_execute.reset();
                    _argument_for_command.resize(0);
                    _parser.Reset();
                }
            }
        }
        _is_alive.store(false);
        if (_read_bytes == 0) {
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
    MutexGetter lock(_mutex);
    _logger->debug("Got mutex for writing");
    auto iov = new iovec[_queue.size()]();

    for (size_t i = 0; i < _queue.size(); ++i) {
        iov[i].iov_base = &(_queue[i][0]);
        iov[i].iov_len = _queue[i].size();
    }

    iov[0].iov_len -= _written;
    iov[0].iov_base = (char *)(iov[0].iov_base) + _written;

    int written_bytes = writev(_socket, iov, _queue.size());

    if (written_bytes == 0 or written_bytes == -1) {
        OnError();
        return;
    }

    size_t done = 0;
    for (auto& command : _queue) {
        if (written_bytes - command.size() < 0) {
            break;
        }
        written_bytes -= command.size();
        done++;
    }

    _queue.erase(_queue.begin(), _queue.begin() + done);
    _written = written_bytes;

    if (_queue.empty()) {
        _event.events = EPOLLET | EPOLLIN | EPOLLERR | EPOLLHUP;
    }
    delete[] iov;
}

} // namespace MTnonblock
} // namespace Network
} // namespace Afina
