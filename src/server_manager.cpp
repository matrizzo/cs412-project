#include <iostream>

#include <arpa/inet.h>
#include <cstring>
#include <sstream>
#include <unistd.h>

#include "grass/grass.h"
#include "grass/network.h"
#include "grass/ring_buffer.h"
#include "grass/server_manager.h"

namespace grass {

ServerManager::ServerManager(const std::string &filename)
    : _config(Config::parseFromFile(filename)), _socket_fd(-1) {}

ServerManager::~ServerManager() {
    if (_socket_fd != -1) {
        CloseSocket(_socket_fd);
    }
}

const std::string &ServerManager::directory() const {
    return _config.directory();
}

bool ServerManager::checkLogin(const std::string &username,
    const std::string &password) const {
    return _config.checkLogin(username, password);
}

std::string ServerManager::loggedUsers() const {
    std::vector<std::string> users;
    users.reserve(_sessions.size());

    for (const auto &session : _sessions) {
        std::string username = session.second.username();
        if (!username.empty()) {
            users.push_back(username);
        }
    }

    sort(users.begin(), users.end());

    std::string result = "";
    for (auto user : users) {
        result += user + " ";
    }

    return result;
}

[[noreturn]] void ServerManager::run() {
    _socket_fd = CreateSocket();
    BindSocket(
        _socket_fd, std::optional<const std::string>(), _config.portNumber());
    StartListeningOnSocket(_socket_fd);

    // Event loop
    while (true) {
        checkForEvents();
    }
}

void ServerManager::checkForEvents() {
    fd_set fds;
    int max_fd = _socket_fd;

    FD_ZERO(&fds);

    // Add the main socket to the list
    FD_SET(_socket_fd, &fds);

    // Add all client sockets
    for (const auto &it : _sessions) {
        FD_SET(it.first, &fds);
        max_fd = std::max(max_fd, it.first);
    }

    /*
     * This will block until there is some data on any of the sockets, or a
     * new connection
     */
    if (select(max_fd + 1, &fds, nullptr, nullptr, nullptr) < 0) {
        throw GrassException("select() failed\n");
    }

    // Check if there is a new connection
    if (FD_ISSET(_socket_fd, &fds)) {
        handleNewConnection();
    }

    // Check if there is any new data from the clients
    for (auto it = _sessions.begin(); it != _sessions.end();) {
        if (FD_ISSET(it->first, &fds)) {
            bool terminate_session = false;
            try {
                terminate_session = it->second.onNewData();
            } catch (const GrassException &e) {
                std::stringstream ss;
                ss << "Error: " << e.what() << '\n';
                std::string msg = ss.str();
                write(it->first, msg.c_str(), msg.length());
                std::cout << ss.str();
            }

            if (terminate_session) {
                /*
                 * In this case we're deleting the session so the iterator
                 * will be invalidated and we need to use the return value
                 * instead
                 */
                it = _sessions.erase(it);
                continue;
            }
        }

        /*
         * If the connection wasn't erased, the iterator is still valid, advance
         * to the next.
         */
        ++it;
    }
}

void ServerManager::handleNewConnection() {
    int new_fd = AcceptFromSocket(_socket_fd);
    SetBlocking(new_fd, false);
    _sessions.insert(std::make_pair(new_fd, Session(*this, new_fd)));
}

} // namespace grass
