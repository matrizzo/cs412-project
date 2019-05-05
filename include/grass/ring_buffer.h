#pragma once

#include <array>
#include <optional>
#include <string>

namespace grass {

/*
 * This class implements a ring buffer that we can use to read data one line
 * at a time from TCP sockets. This is needed because the GRASS protocol is
 * line-based (processes one line at a time) but TCP sockets expose a continuous
 * stream of data, and a single read from a socket might return only part of a
 * line, or multiple lines.
 *
 * The buffer uses a fixed-size array and two indices into this array (read_idx
 * and write_idx). read_idx is the index of the first element to be read, and
 * write_idx is the index of the first element to be written.
 *
 * By convention the buffer is empty when read_idx == write_idx and full when
 * write_idx == read_idx - 1 (mod SIZE).
 *
 * The user of the ring buffer calls nextLine, which returns the next line read
 * from the buffer (or nothing if there are no complete lines in the buffer)
 */
class RingBuffer {
public:
    // Constructs a new buffer
    RingBuffer() : _read_idx(0), _write_idx(0) {}

    /*
     * Fetches the next input line from the buffer. If there are no complete
     * lines in the buffer, this function will first attempt to read more data
     * from socket_fd. Returns the first full line in the buffer, or an empty
     * std::optional if there are no full lines in the buffer. Sets
     * socket_closed to true if the other side has closed the socket.
     */
    std::optional<std::string> nextLine(int socket_fd, bool &socket_closed);

private:
    /*
     * Size of the buffer, in bytes. The amount of data in the buffer at any
     * time will be at most 1 byte less than this so that it's easier
     * to distinguish between a full buffer and an empty buffer.
     */
    static const size_t SIZE = 4096;

    // Checks if the buffer is empty
    bool empty() const;
    // Checks if the buffer is full
    bool full() const;

    // Returns the index of the first newline character in the buffer
    std::optional<size_t> findNewline() const;
    /*
     * Fills the buffer by reading from the socket. Sets socket_closed to true
     * if the other side has closed the socket.
     */
    void receiveFromSocket(int socket_fd, bool &socket_closed);
    /*
     * Removes data from the buffer starting at read_index and finishing at end
     * and returns it.
     */
    std::string consume(size_t end);

    // Read and write indices
    size_t _read_idx;
    size_t _write_idx;

    // Data buffer
    std::array<char, SIZE> _buf;
};

} // namespace grass
