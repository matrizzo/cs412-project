#pragma once

// Required because Session references ServerManager
namespace grass {

class ServerManager;

} // namespace grass

#include <unordered_map>

#include "config.h"
#include "session.h"

namespace grass {

/*
 * This class implements the GRASS server. It owns the listening socket, accepts
 * new connections and dispatches incoming data on the data sockets to the
 * session objects. The event loop of the server is located here
 */
class ServerManager {
public:
    /*
     * Constructs an instance of the server from the path to the
     * configuration file
     */
    ServerManager(const std::string &filename);

    // Destructor which closes the listening socket
    ~ServerManager();

    // Returns the path of the base directory of the server
    const std::string &directory() const;

    // Checks if a (username, password) combination is valid
    bool checkLogin(const std::string &username,
        const std::string &password) const;

    /*
     * Returns the list of users that are currently logged into the server,
     * sorted in alphabetical order
     */
    std::string loggedUsers() const;

    // Runs the event loop of the server, which never terminates
    [[noreturn]] void run();

private:
    // Checks if there are any new connections or any data from any client
    void checkForEvents();

    // Handles a new connection from one of the clients
    void handleNewConnection();

    // Creates a new listening socket for the server
    void createSocket();

    // The server's configuration
    const Config _config;

    // The file descriptor for the server's listening socket
    int _socket_fd;

    /*
     * Maps the file descriptor of a session's data socket to the session
     * itself
     */
    std::unordered_map<int, Session> _sessions;
};

} // namespace grass
