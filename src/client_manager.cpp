#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

#include "grass/client_manager.h"
#include "grass/filesystem.h"
#include "grass/grass.h"
#include "grass/network.h"

namespace grass {

ClientManager::ClientManager(const char *address, uint16_t port)
    : _socket(CreateSocket()), _interactive(true), _server_addr(address),
      _server_port(port) {

    // Set stdin to be non-blocking
    int stdin_flags = fcntl(0, F_GETFL);
    if (stdin_flags < 0) {
        throw GrassException("fcntl() failed");
    }
    if (fcntl(0, F_SETFL, stdin_flags | O_NONBLOCK) < 0) {
        throw GrassException("fcntl() failed");
    }
}

ClientManager::ClientManager(const char *address,
    uint16_t port,
    const std::string &input,
    const std::string &output)
    : _socket(CreateSocket()), _interactive(false), _server_addr(address),
      _server_port(port) {

    // Redirect stdin
    if (!freopen(input.c_str(), "r", stdin)) {
        throw GrassException("Error opening input file.");
    }

    // Redirect stdout
    if (!freopen(output.c_str(), "w", stdout)) {
        throw GrassException("Error opening ouput file.");
    }
}

ClientManager::~ClientManager() {
    if (_socket > 0) {
        ShutdownSocket(_socket);
        CloseSocket(_socket);
    }
}

void ClientManager::run() {
    // Connect to the server
    ConnectToSocket(_socket, _server_addr, _server_port, true);

    bool input_closed = false;
    bool connection_closed = false;

    grass::RingBuffer input, output;
    fd_set rdfs;

    if (_interactive) {
        write(1, "> ", 2);
    }

    // Interactive mode: loop until when the socket or stdin are closed
    // Non-interactive mode: loop until when the socket is closed
    while (!connection_closed && !(_interactive && input_closed)) {

        // Set up select() to check the socket and stdin (if still open)
        FD_ZERO(&rdfs);
        FD_SET(_socket, &rdfs);
        if (!input_closed) {
            FD_SET(0, &rdfs);
        }

        // This will block until there is data on either the socket or stdin
        if (select(_socket + 1, &rdfs, nullptr, nullptr, nullptr) < 0) {
            throw GrassException("select() failed");
        }

        if (FD_ISSET(0, &rdfs)) {
            // Data on stdin
            input_closed = handleInput(input);
        } else {
            // Data on socket
            connection_closed = handleOutput(output);

            if (_interactive && !input_closed && !connection_closed) {
                write(1, "> ", 2);
            }
        }
    }
}

bool ClientManager::handleInput(RingBuffer &buffer) {
    // Verify whether the input stream is still open
    if (IsClosed(0)) {
        return true;
    }

    std::optional<std::string> maybeLine;
    bool closed = false;

    // Read as much data as possible from the input stream
    while (bool(maybeLine = buffer.nextLine(0, closed))) {
        std::string line = maybeLine.value();

        try {
            if (line.length() >= 3 && line.substr(0, 3) == "get") {
                // Handle send
                sendGetRequest(line.substr(3));
            } else if (line.length() >= 3 && line.substr(0, 3) == "put") {
                // Handle get
                sendPutRequest(line.substr(3));
            } else {
                // Handle other inputs
                line += '\n';
                write(_socket, line.c_str(), line.length());
            }
        } catch (const GrassException &e) {
            std::stringstream ss;
            ss << "Error: " << e.what() << '\n';
            std::string msg = ss.str();
            write(1, msg.c_str(), msg.length());

            if (_interactive) {
                write(1, "> ", 2);
            }
        }
    }

    return closed;
}

bool ClientManager::handleOutput(RingBuffer &buffer) {
    // Verify whether `_socket` is still open
    if (IsClosed(_socket)) {
        return true;
    }

    std::optional<std::string> maybeLine;
    bool closed = false;

    // Read as much data as possible from `_socket`
    while (bool(maybeLine = buffer.nextLine(_socket, closed))) {
        std::string line = maybeLine.value();

        try {
            if (line.length() >= 10) {
                std::string substring = line.substr(0, 10);

                if (substring == "Error: get") {
                    // Handle get error
                    _files_to_receive.pop();
                } else if (substring == "Error: put") {
                    // Handle put error
                    _files_to_send.pop();
                } else if (substring == "get port: ") {
                    // Handle get
                    receiveFile(line.substr(10));
                    continue;
                } else if (substring == "put port: ") {
                    // Handle put
                    sendFile(line.substr(10));
                    continue;
                }
            }

            if (line == "exit") {
                // If "exit" was received, set closed flag
                closed = true;
            } else if (!line.empty()) {
                // Handle other messages
                write(1, (line + '\n').c_str(), line.length() + 1);
            }
        } catch (const GrassException &e) {
            std::stringstream ss;
            ss << "Error: " << e.what() << '\n';
            std::string msg = ss.str();
            write(1, msg.c_str(), msg.length());
        }
    }

    return closed;
}

void ClientManager::sendGetRequest(const std::string &params) {
    // Minimum length 2 (one character for the space and at least one for
    // the file name)
    if (params.length() < 2 || params[0] != ' ') {
        throw GrassException("Wrong arguments for get");
    }

    // Extract the path of the target file on the server
    std::string path = params.substr(1);

    // Save the filename we requested for later. Requested file are saved
    // inside the directory from which the client was launched.
    std::string filename = ExtractFilename(path);
    _files_to_receive.push(filename);

    // Forward the command to the server
    std::string server_command = "get " + path + "\n";
    write(_socket, server_command.c_str(), server_command.length());
}

void ClientManager::sendPutRequest(const std::string &params) {
    // Minimum length 4 (two spaces, at least one each for filename and
    // length)
    if (params.length() < 4 || params[0] != ' ') {
        throw GrassException("Wrong arguments for put");
    }

    std::string params_sub1 = params.substr(1);

    size_t second_space_index = params_sub1.find(' ');
    if (second_space_index == std::string::npos ||
        params_sub1.length() < second_space_index + 2) {
        throw GrassException("Wrong arguments for put");
    }

    std::string filename = params_sub1.substr(0, second_space_index);
    if (!S_ISREG(Stat(filename).st_mode)) {
        throw GrassException("Not a file");
    }
    _files_to_send.push(filename);

    // Forward the command to the server
    std::string server_command = "put " + params + "\n";
    write(_socket, server_command.c_str(), server_command.length());
}

void ClientManager::receiveFile(const std::string &params) {
    std::string file_name = _files_to_receive.front();
    _files_to_receive.pop();

    size_t pos = 0;

    unsigned long port_ul = std::stoul(params, &pos);
    if (port_ul > 65535) {
        throw GrassException("get: invalid port");
    }
    uint16_t port = static_cast<uint16_t>(port_ul);

    std::string size_str = params.substr(pos);
    if (size_str.substr(0, 7) != " size: ") {
        throw GrassException("get: invalid size");
    }

    size_str = size_str.substr(7);
    unsigned long file_size = std::stoul(size_str);

    int file_fd = open(file_name.c_str(),
        O_WRONLY | O_NOFOLLOW | O_CREAT | O_TRUNC,
        S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (file_fd < 0) {
        switch (errno) {
        case ENOENT:
            throw GrassException("No such file or directory");
        default:
            throw GrassException("Failed to open file for get()");
        }
    }

    std::thread worker(receiveFileWorker,
        file_fd,
        _server_addr,
        port,
        static_cast<size_t>(file_size));

    worker.detach();
}

void ClientManager::receiveFileWorker(int file_fd,
    std::string addr,
    uint16_t port,
    size_t file_size) {
    int socket = -1;
    size_t total_received_bytes = 0;

    try {
        socket = CreateSocket();
        ConnectToSocket(socket, addr, port, false);

        char buffer[4096];

        ssize_t read_bytes = read(socket, buffer, sizeof(buffer));
        while (read_bytes > 0) {

            write(file_fd, buffer, static_cast<size_t>(read_bytes));

            total_received_bytes += static_cast<size_t>(read_bytes);
            if (total_received_bytes > file_size) {
                break;
            }

            read_bytes = read(socket, buffer, sizeof(buffer));
        }

        ShutdownSocket(socket);
        CloseSocket(socket);
    } catch (...) {
        if (socket >= 0) {
            ShutdownSocket(socket);
            CloseSocket(socket);
        }
    }

    close(file_fd);
}

void ClientManager::sendFile(const std::string &params) {
    std::string file_name = _files_to_send.front();
    _files_to_send.pop();

    unsigned long port_ul = std::stoul(params);
    if (port_ul > 65535) {
        std::cout << "put: invalid port " << port_ul;
        return;
    }
    uint16_t port = static_cast<uint16_t>(port_ul);

    int file_fd = open(file_name.c_str(), O_RDONLY | O_NOFOLLOW);
    if (file_fd < 0) {
        switch (errno) {
        case ENOENT:
            throw GrassException("No such file or directory");
        default:
            throw GrassException("Failed to open file for put()");
        }
    }

    std::thread worker(sendFileWorker, file_fd, _server_addr, port);
    worker.detach();
}

void ClientManager::sendFileWorker(int file_fd,
    std::string addr,
    uint16_t port) {
    int socket = -1;

    struct stat stat;

    if (fstat(file_fd, &stat) < 0) {
        close(file_fd);
        return;
    }

    size_t file_size = static_cast<size_t>(stat.st_size);

    try {
        socket = CreateSocket();
        ConnectToSocket(socket, addr, port, false);

        sendfile(socket, file_fd, nullptr, file_size);

        ShutdownSocket(socket);
        CloseSocket(socket);
    } catch (...) {
        if (socket >= 0) {
            ShutdownSocket(socket);
            CloseSocket(socket);
        }
    }

    close(file_fd);
}
} // namespace grass
