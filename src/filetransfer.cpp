#include "grass/filetransfer.h"
#include "grass/grass.h"
#include "grass/network.h"

#include <exception>
#include <fcntl.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

#define BUFFER_SIZE 4096

namespace grass {

FileTransfer::FileTransfer(const std::string &path) : _is_get(true) {
    // Get
    int file_fd = open(path.c_str(), O_RDONLY | O_NOFOLLOW);
    if (file_fd < 0) {
        throw GrassException("Failed to open file for get()");
    }

    struct stat stat;

    if (fstat(file_fd, &stat) < 0) {
        throw GrassException("Failed to get file size");
    }

    _file_fd = file_fd;
    _file_size = static_cast<size_t>(stat.st_size);
}

FileTransfer::FileTransfer(const std::string &path, size_t size)
    : _file_size(size), _is_get(false) {
    // Put
    int file_fd = open(path.c_str(),
        O_WRONLY | O_NOFOLLOW | O_CREAT | O_TRUNC,
        S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (file_fd < 0) {
        throw GrassException("Failed to open file for put()");
    }

    _file_fd = file_fd;
}

uint16_t FileTransfer::run(size_t &file_size) const {
    try {
        int socket = CreateSocket();

        BindSocket(socket, std::optional<std::string>(), 0);
        uint16_t port = GetSocketPort(socket);
        StartListeningOnSocket(socket);

        std::thread worker(
            transferWorker, socket, _file_fd, _file_size, _is_get);
        worker.detach();

        file_size = _file_size;
        return port;
    } catch (...) {
        close(_file_fd);
        std::rethrow_exception(std::current_exception());
    }
}

void FileTransfer::transferWorker(int socket,
    int file_fd,
    size_t file_size,
    bool is_get) {
    try {
        int data_socket = AcceptFromSocket(socket);

        if (is_get) {
            // get
            sendfile(data_socket, file_fd, nullptr, file_size);
        } else {
            // put
            char buf[BUFFER_SIZE];
            ssize_t received_bytes = read(data_socket, buf, 4200);
            printf("Size is %d", strlen(buf));
            size_t total_bytes = 0;

            while (received_bytes > 0) {
                write(file_fd, buf, static_cast<size_t>(received_bytes));

                total_bytes += static_cast<size_t>(received_bytes);
                if (total_bytes >= file_size) {
                    break;
                }

                received_bytes = read(data_socket, buf, sizeof(buf));
            }
        }

        ShutdownSocket(data_socket);
        CloseSocket(data_socket);

    } catch (...) {
    }

    ShutdownSocket(socket);
    CloseSocket(socket);
    close(file_fd);
}

} // namespace grass
