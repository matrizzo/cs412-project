#pragma once

// This is needed because ServerManager references Session
namespace grass {
class Session;
}

#include <functional>
#include <optional>
#include <regex.h>
#include <string>
#include <variant>
#include <vector>

#include "config.h"
#include "filesystem.h"
#include "ring_buffer.h"
#include "server_manager.h"

namespace grass {

class Session {
    // List of privileged commands
    static const std::array<std::string, 11> PRIVILEGED_COMMANDS;

    // The state of the session (whether someone is logged in)
    struct state {
        std::optional<std::string> username;
        std::optional<std::string> login_attempt;
    };
    state _state;

    // A reference back to the server
    const ServerManager &_server;

    // The directory in which the user is currently located
    Directory _directory;

    // Buffer for incoming data
    RingBuffer _buffer;

    // Data socket to communicate with the client
    int _socket_fd;

public:
    /*
     * Constructs a new session from a server and a file descriptor for the data
     * socket
     */
    Session(const ServerManager &server, int socket_fd);

    // Destructor which closes the session's data socket
    ~Session();

    // Delete the copy constructors for session
    Session(const Session &other) = delete;
    Session &operator=(const Session &other) = delete;

    // Move constructor, takes ownership of the data socket
    Session(Session &&other);

    // Returns the user logged into this session
    std::string username() const;

    // Reads and process new data from the socket. Returns true if the
    // connection should be closed.
    bool onNewData();

private:
    /*
     * Executes a command in the context of this session. Returns the output of
     * the command.
     */
    std::string execute(const std::string &cmd);

    /*
     * Executes a privileged command (i.e. one that requires the user to be
     * logged in). Returns the output of the command.
     */
    std::string executePrivileged(const std::vector<std::string> &cmd);

    // Each command has its own execute method.
    std::string executePass(const std::string &pass);
    std::string executePing(const std::string &host) const;
    std::string executeLs() const;
    std::string executeCd(const std::string &path);
    std::string executeMkdir(const std::string &path) const;
    std::string executeRm(const std::string &path) const;
    std::string executeGet(const std::string &filename) const;
    std::string executePut(const std::string &path, size_t size) const;
    std::string executeGrep(const std::string &pattern) const;
    std::string executeDate() const;
    std::string executeWhoami() const;
    std::string executeW() const;
    std::string executeLogout();

    /*
     * Verifies that the given list of arguments contains at least two elements
     * and that the second element is not longer than the maximum path length
     */
    void ValidatePathArg(const std::vector<std::string> &args);
};

} // namespace grass
