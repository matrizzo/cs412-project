#include "grass/network.h"
#include "grass/grass.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>

namespace grass {

/*
 * Size of the connection backlog for listening sockets, i.e. how many
 * incoming connections can be pending before the system should stop accepting
 * them
 */
static const int SOCKET_BACKLOG_SIZE = 128;

int CreateSocket() {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        throw GrassException("Failed to create socket");
    }

    int enable = 1;
    if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) <
        0) {
        throw GrassException("Failed to set socket options");
    }

    return socket_fd;
}

void BindSocket(int socket,
    const std::optional<const std::string> &addr,
    uint16_t port) {
    if (socket < 0) {
        throw GrassException("Cannot call `Bind` on a closed `Socket`");
    }

    sockaddr_in sockaddr;
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(port);

    if (bool(addr)) {
        if (inet_pton(AF_INET, addr.value().c_str(), &sockaddr.sin_addr) <= 0) {
            throw GrassException("Invalid IP address");
        }
    } else {
        sockaddr.sin_addr.s_addr = INADDR_ANY;
    }

    std::memset(sockaddr.sin_zero, 0, sizeof(sockaddr.sin_zero));

    if (bind(socket,
            reinterpret_cast<const struct sockaddr *>(&sockaddr),
            sizeof(sockaddr_in)) < 0) {
        throw GrassException("Bind failed");
    }
}

uint16_t GetSocketPort(int socket) {
    struct sockaddr_in sockaddr;
    socklen_t len_inet = sizeof(sockaddr);

    if (getsockname(socket,
            reinterpret_cast<struct sockaddr *>(&sockaddr),
            &len_inet) < 0) {
        throw GrassException("Random port allocation failed");
    }

    return ntohs(sockaddr.sin_port);
}

void ConnectToSocket(int socket,
    const std::string &addr,
    uint16_t port,
    bool nonblocking) {
    sockaddr_in sockaddr;
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(port);
    std::memset(sockaddr.sin_zero, 0, sizeof(sockaddr.sin_zero));

    if (inet_pton(AF_INET, addr.c_str(), &sockaddr.sin_addr) <= 0) {
        throw GrassException("Invalid IP address");
    }

    ssize_t status = connect(socket,
        reinterpret_cast<const struct sockaddr *>(&sockaddr),
        sizeof(sockaddr_in));

    if (status && errno != EINPROGRESS) {
        throw GrassException("Connect failed");
    }

    SetBlocking(socket, !nonblocking);
}

void StartListeningOnSocket(int socket) {
    if (listen(socket, SOCKET_BACKLOG_SIZE) < 0) {
        throw GrassException("Listen failed");
    }
}

int AcceptFromSocket(int socket) {
    sockaddr_in remote;
    socklen_t size = sizeof(sockaddr_in);

    int descriptor =
        accept(socket, reinterpret_cast<struct sockaddr *>(&remote), &size);

    if (descriptor < 0)
        throw GrassException("Accept failed");

    return descriptor;
}

void ShutdownSocket(int socket) { shutdown(socket, SHUT_RDWR); }

void CloseSocket(int socket) { close(socket); }

void SetBlocking(int socket, bool blocking) {
    int socket_flags = fcntl(socket, F_GETFL);
    if (socket_flags < 0) {
        throw GrassException("fcntl() failed");
    }

    if (!blocking) {
        socket_flags |= O_NONBLOCK;
    } else {
        socket_flags &= ~O_NONBLOCK;
    }

    if (fcntl(socket, F_SETFL, socket_flags) < 0) {
        throw GrassException("fcntl() failed");
    }
}

} // namespace grass
