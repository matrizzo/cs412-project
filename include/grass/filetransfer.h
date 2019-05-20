#pragma once

#include <string>

namespace grass {

// Class for a file transfer
class FileTransfer {
public:
    // Constructs a file transfer that sends data to the client (for get)
    FileTransfer(const std::string &path);

    // Constructs a file transfer that receives data to the client (for get)
    FileTransfer(const std::string &path, size_t size);

    /*
     * Runs the file transfer, returning the port which the client should
     * connect to. Fills in the size of the file being transfered
     */
    uint16_t run(size_t &file_size) const;

private:
    /*
     * Worker thread for the file transfer, which sends or receives the file.
     * This thread is responsible for closing the file descriptors.
     */
    static void transferWorker(int socket, int file_fd, size_t file_size, bool is_get, std::string filename);

    int _file_fd;
    size_t _file_size;
    bool _is_get;
    std::string _filename;
};

} // namespace grass
