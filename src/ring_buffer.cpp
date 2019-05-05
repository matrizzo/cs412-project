#include "grass/ring_buffer.h"
#include "grass/grass.h"

#include <cerrno>

#include <sys/socket.h>
#include <unistd.h>

namespace grass {

std::optional<std::string> RingBuffer::nextLine(int fd, bool &socket_closed) {
    auto nextNewLine = findNewline();

    if (nextNewLine) {
        return consume(nextNewLine.value());
    }

    // Get more data from the socket
    receiveFromSocket(fd, socket_closed);

    // Now that we have received some data, try again to find a newline
    nextNewLine = findNewline();
    if (nextNewLine) {
        return consume(nextNewLine.value());
    }

    return {};
}

void RingBuffer::receiveFromSocket(int fd, bool &socket_closed) {
    // If the buffer is full don't receive any more data
    if (full()) {
        return;
    }

    socket_closed = false;

    if (_write_idx < _read_idx) {
        // In this case we can do a straight-line (non-wrapping) read
        size_t max_recv_size = _read_idx - _write_idx - 1;
        ssize_t received_size = read(fd, &_buf[_write_idx], max_recv_size);
        if (received_size < 0) {
            if (errno == EAGAIN) {
                // The socket was non-blocking and no data was available
                return;
            }
            throw GrassException("read() failed");
        } else if (received_size == 0) {
            /*
             * read() returns 0 on EOF (in the case of a socket, when the socket
             * is closed).
             */
            socket_closed = true;
            return;
        }

        /*
         * The read will not wrap past the end of the buffer so modulo is not
         * needed
         */
        _write_idx += static_cast<size_t>(received_size);
    } else {
        /*
         * In this case the read will wrap around the end of the buffer so it
         * has to be done in two stages. One from the current write pointer to
         * the end of the buffer, and a second from the start of the buffer to
         * the read pointer
         */
        size_t max_recv_size = SIZE - _write_idx;
        ssize_t received_size = read(fd, &_buf[_write_idx], max_recv_size);
        if (received_size < 0) {
            if (errno == EAGAIN) {
                return;
            }
            throw GrassException("read() failed");
        } else if (received_size == 0) {
            socket_closed = true;
            return;
        }

        _write_idx = (_write_idx + static_cast<size_t>(received_size)) % SIZE;

        if (max_recv_size == static_cast<size_t>(received_size)) {
            /*
             * If the OS did not return less data than what we asked it means
             * that there might be more data available on the socket. In that
             * case we need to do the second part of the read. We use recursion
             * for that, because we know that we will end up in the base case
             */
            receiveFromSocket(fd, socket_closed);
        }
    }
}

std::optional<size_t> RingBuffer::findNewline() const {
    for (size_t i = _read_idx; i != _write_idx; i = (i + 1) % SIZE) {
        if (_buf[i] == '\n') {
            return i;
        }
    }

    return {};
}

std::string RingBuffer::consume(size_t end) {
    if (end >= _read_idx) {
        std::string ret(&_buf[_read_idx], end - _read_idx);

        _read_idx = (end + 1) % SIZE;
        return ret;
    }

    std::string ret(&_buf[_read_idx], SIZE - _read_idx);
    _read_idx = 0;

    return ret + consume(end);
}

bool RingBuffer::empty() const {
    return _read_idx == _write_idx;
}

bool RingBuffer::full() const {
    return _read_idx == (_write_idx + 1) % SIZE;
}

} // namespace grass
