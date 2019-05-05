#pragma once

#include <optional>
#include <string>

#include "grass/ring_buffer.h"

namespace grass {

// Creates a new TCP socket for the IPv4 protocol
int CreateSocket();

/*
 * Binds a name (address and port) to a socket. If addr is an empty optional,
 * the socket will be bound to all available interfaces (INADDR_ANY). This is
 * used for server-side sockets, on which we will accept connections. If port is
 * 0 the socket will be bound to a random port
 */
void BindSocket(int socket,
    const std::optional<const std::string> &addr,
    uint16_t port);

/*
 * Connect a socket to another socket, identified by (addr, port), i.e. an IP
 * address and a port. This is used on the client side to connect to a server.
 * When nonblocking == true the socket will be made non-blocking
 */
void ConnectToSocket(int socket,
    const std::string &addr,
    uint16_t port,
    bool nonblocking = false);

/*
 * Get the port to which the socket is bound. This useful when the socket was
 * bound to a random port
 */
uint16_t GetSocketPort(int socket);

/*
 * Sets a socket to be blocking or non-blocking. Operations on a blocking socket
 * will block the calling thread if they can't be executed immediately (e.g.
 * trying to read from a socket when no data is avaiable)
 */
void SetBlocking(int socket, bool blocking);

/*
 * Tell the operating system that we want to start listening for incoming
 * connections on this socket
 */
void StartListeningOnSocket(int socket);

// Accept an incoming connection on a listening socket
int AcceptFromSocket(int socket);

/*
 * Signal to the other side of the connection that no more data will be read or
 * written on this socket
 */
void ShutdownSocket(int socekt);

/*
 * Close the socket, releasing any operating system resources
 */
void CloseSocket(int socket);

} // namespace grass
