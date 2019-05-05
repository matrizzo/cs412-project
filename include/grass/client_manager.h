#pragma once

#include <fstream>
#include <functional>
#include <queue>
#include <string>

#include "grass/ring_buffer.h"

namespace grass {

/*
 * This class implements the GRASS client
 */
class ClientManager {
public:
    // Constructs an interactive client
    ClientManager(const char *address, uint16_t port);

    // Constructs a batch client
    ClientManager(const char *address,
        uint16_t port,
        const std::string &input,
        const std::string &output);

    // Destroys the client (closes the socket)
    ~ClientManager();

    // Runs the client. This function contains the client's event loop
    void run();

private:
    // Called when the client receives data from the input
    bool handleInput(RingBuffer &buffer);

    // Called when the client receives data from the server
    bool handleOutput(RingBuffer &buffer);

    // Sends a get request to the server
    void sendGetRequest(const std::string &params);

    // Sends a put request to the server
    void sendPutRequest(const std::string &params);

    // Receives a file from the server
    void receiveFile(const std::string &params);

    // Sends a file to the server
    void sendFile(const std::string &params);

    /*
     * Worker functions that runs in a dedicated thread and receives a file from
     * the server
     */
    static void receiveFileWorker(int file_fd,
        std::string addr,
        uint16_t port,
        size_t file_size);

    /*
     * Worker functions that runs in a dedicated thread and sends a file to the
     * server
     */
    static void sendFileWorker(int file_fd, std::string addr, uint16_t port);

    // Socket to communicate with the server
    int _socket;

    /*
     * Whether the client is being used in interactive mode (with input from a
     * console)
     */
    bool _interactive;

    // IP address and port of the server to which the client should connect
    std::string _server_addr;
    uint16_t _server_port;

    // Queues of file names to send and receive
    std::queue<std::string> _files_to_send;
    std::queue<std::string> _files_to_receive;
};

} // namespace grass
