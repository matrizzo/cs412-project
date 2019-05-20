#include <iostream>

#include <arpa/inet.h>
#include <array>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fts.h>
#include <iterator>
#include <memory>
#include <sstream>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "grass/filetransfer.h"
#include "grass/grass.h"
#include "grass/grep.h"
#include "grass/network.h"
#include "grass/server_manager.h"
#include "grass/session.h"

#define ERROR_LENGTH 2048

namespace grass {

const std::array<std::string, 11> Session::PRIVILEGED_COMMANDS = {"ls",
    "cd",
    "mkdir",
    "rm",
    "get",
    "put",
    "grep",
    "date",
    "whoami",
    "w",
    "logout"};

Session::Session(const ServerManager &server, int socket_fd)
    : _server(server), _directory(server.directory()), _socket_fd(socket_fd) {}

Session::~Session() {
    if (_socket_fd != -1) {
        ShutdownSocket(_socket_fd);
        CloseSocket(_socket_fd);
    }
}

Session::Session(Session &&other)
    : _state{other._state.username, other._state.login_attempt},
      _server(other._server), _directory(other._directory),
      _socket_fd(other._socket_fd) {

    other._socket_fd = -1;
}

std::string Session::username() const {
    if (_state.username) {
        return _state.username.value();
    } else {
        return "";
    }
}

void log(std::string err_message) {
    char log_message[ERROR_LENGTH];
    strcpy(log_message, err_message.c_str());
    fprintf(stderr, "%s\n", log_message);
}

bool Session::onNewData() {
    std::optional<std::string> cmd;

    bool socket_closed = false;
    std::string response;

    // Keep reading and executing new commands. Stop only when either the buffer
    // is drained or the other side closed the connection.
    while (bool(cmd = _buffer.nextLine(_socket_fd, socket_closed))) {
        if (cmd) {
            // We received "exit", close this connection
            if (cmd == "exit") {
                return true;
            }

            // Execute the command requested by the user
            try {
                response = execute(cmd.value());
            } catch (const GrassException &e) {
                // Extract command (removing arguments)
                std::string err_cmd =
                    cmd.value().substr(0, cmd.value().find(" "));

                // Log error
                log(err_cmd + "\n");

                // Build error message
                response = "Error: " + err_cmd + ": " + e.what() + "\n";
            }

            // Print response if not empty
            if (response != "") {
                std::cout << response << std::endl;
            }

            response += "\n";
            write(_socket_fd, response.c_str(), response.length());
        }
    }

    return socket_closed;
}

std::string Session::execute(const std::string &cmd) {
    // The command must not be empty.
    if (cmd.empty()) {
        return "";
    }

    // Split the command in a vector of space-separated arguments
    std::istringstream stream(cmd);
    std::vector<std::string> args{std::istream_iterator<std::string>{stream},
        std::istream_iterator<std::string>{}};

    // The command must not only contain whitespace
    if (args.empty()) {
        return "";
    }

    // If there is an ongoing login attempt, the next command has to be "pass"
    if (_state.login_attempt) {
        if (args[0] != "pass") {
            // Reset login state
            _state.login_attempt.reset();
            return "Login interrupted";
        }

        if (args.size() < 2) {
            return "Missing password";
        }

        return executePass(args[1]);
    }

    // Start login attempt
    if (args[0] == "login") {
        if (args.size() < 2) {
            return "Missing username";
        }

        _state.login_attempt = args[1];
        return "";
    }

    // Ping a host
    if (args[0] == "ping") {
        if (args.size() < 2) {
            return "Missing host";
        }

        return executePing(args[1]);
    }

    if (args[0] == "exit") {
        return "";
    }

    // Only logged in users are allowed to execute privileged commands
    if (std::find(PRIVILEGED_COMMANDS.begin(),
            PRIVILEGED_COMMANDS.end(),
            args[0]) != PRIVILEGED_COMMANDS.end()) {

        if (_state.username) {
            return executePrivileged(args);
        } else {
            throw GrassException("Access denied");
        }
    }

    // At this point no command matched: the command is invalid
    throw GrassException("Invalid command");
}

std::string Session::executePrivileged(const std::vector<std::string> &args) {
    if (args[0] == "ls") {
        return executeLs();
    }

    if (args[0] == "cd") {
        ValidatePathArg(args);
        return executeCd(args[1]);
    }

    if (args[0] == "mkdir") {
        ValidatePathArg(args);
        return executeMkdir(args[1]);
    }

    if (args[0] == "rm") {
        ValidatePathArg(args);
        return executeRm(args[1]);
    }

    if (args[0] == "get") {
        ValidatePathArg(args);
        return executeGet(args[1]);
    }

    if (args[0] == "put") {
        // Validate arguments
        if (args.size() < 3) {
            throw GrassException("Missing arguments");
        }
        if (args[1].length() > MAX_PATH_LENGTH) {
            throw GrassException("The path is too long");
        }

        // Parse file size
        size_t size = std::strtoul(args[2].c_str(), nullptr, 10);
        if (size == 0) {
            throw GrassException("Invalid size");
        }

        return executePut(args[1], size);
    }

    if (args[0] == "grep") {
        // Validate argument
        if (args.size() < 2) {
            throw GrassException("Missing pattern");
        }

        return executeGrep(args[1]);
    }

    if (args[0] == "date") {
        return executeDate();
    }

    if (args[0] == "whoami") {
        return executeWhoami();
    }

    if (args[0] == "w") {
        return executeW();
    }

    if (args[0] == "logout") {
        return executeLogout();
    }

    // At this point no command matched: the command is invalid
    throw GrassException("Invalid command");
}

std::string Session::executePass(const std::string &pass) {
    if (!_server.checkLogin(_state.login_attempt.value(), pass)) {
        // Wrong password
        _state.login_attempt.reset();
        return "Wrong credentials";
    }

    // Login succeeded
    _state.username.reset();
    _state.username.swap(_state.login_attempt);
    return "";
}

std::string Session::executePing(const std::string &host) const {

    // A valid hostname contains only letters (a-z), digits (0-9), dots (.)
    // and dashes (-). Source: https://en.wikipedia.org/wiki/Hostname
    // In order to support IPv6 addresses we also allow colons (:).
    if (host.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789.-:") !=
        std::string::npos) {

        throw GrassException("Invalid hostname");
    }

    // Execute Ping
    std::array<char, 128> buffer;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(
        popen(("ping -c 1 " + host + " 2>&1").c_str(), "r"), pclose);
    if (!pipe) {
        throw GrassException("ping() failed");
    }

    // Build output string
    std::string result;
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    return result;
}

std::string Session::executeLs() const {
    std::string path = _directory.PathFromRoot();
    std::array<char, 128> buffer;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(
        popen(("ls -l '" + path + "'").c_str(), "r"), pclose);
    if (!pipe) {
        throw GrassException("ls() failed");
    }

    // Build output string
    std::string result;
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    return result;
}

std::string Session::executeCd(const std::string &relative_path) {
    // Parse and validate argument
    std::vector<std::string> path_vector =
        _directory.TokenizePath(relative_path);
    std::string path_string = _directory.PathFromVector(path_vector);
    if (!S_ISDIR(Stat(path_string).st_mode)) {
        throw GrassException("Not a directory");
    }

    // Execute cd
    _directory.Update(path_vector);

    return "";
}

std::string Session::executeMkdir(const std::string &relative_path) const {

    std::string valid_characters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_";
    if (relative_path.find_first_not_of(valid_characters) != std::string::npos) {

        throw GrassException("Invalid directory name");
    }

    // Build absolute path and permissions mask
    std::string path = _directory.PathFromRoot(relative_path);
    mode_t mask = S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;

    // Execute mkdir
    if (mkdir(path.c_str(), mask)) {
        switch (errno) {
        case ENOENT:
            throw GrassException("No such file or directory");
        case EEXIST:
            throw GrassException("Directory already exists");
        default:
            throw GrassException("mkdir() failed");
        }
    }

    return "";
}

std::string Session::executeRm(const std::string &relative_path) const {
    // Validate argument
    if (relative_path == "." || relative_path == "..") {
        throw GrassException("Refusing to remove '.' or '..' directory");
    }

    // Verify path exists
    std::string path = _directory.PathFromRoot(relative_path);
    Stat(path);

    // Execute rm
    RemoveDirectoryRecursively(path);

    return "";
}

std::string Session::executeGet(const std::string &filename) const {
    // Validate filename
    std::string path = _directory.PathFromRoot(filename);
    if (!S_ISREG(Stat(path).st_mode)) {
        throw GrassException("Not a file");
    }

    // Create `FileTransfer` and start it
    size_t file_size;
    FileTransfer transfer(path);
    uint16_t port = transfer.run(file_size);

    // Return port and size to the client
    std::stringstream ss;
    ss << "get port: " << port << " size: " << file_size;
    return ss.str();
}

std::string Session::executePut(const std::string &path, size_t size) const {
    // The file will be saved in the current working directory
    std::string filename = ExtractFilename(path);
    std::string absolute_path = _directory.PathFromRoot(filename);

    // Create `FileTransfer` and start it
    size_t file_size = size;
    FileTransfer transfer(absolute_path, file_size);
    uint16_t port = transfer.run(file_size);

    // Return port to the client
    std::stringstream ss;
    ss << "put port: " << port;
    return ss.str();
}

std::string Session::executeGrep(const std::string &pattern) const {
    // Execute grep
    std::string path = _directory.PathFromRoot();
    std::vector<std::string> files_found = SearchDirectory(path, pattern);

    // Sort filenames
    std::sort(files_found.begin(), files_found.end());

    // Build output string
    std::stringstream ss;
    for (const auto &f : files_found) {
        ss << f << '\n';
    }

    return ss.str();
}

std::string Session::executeDate() const {
    // Get current time
    timeval time;
    gettimeofday(&time, nullptr);
    time_t seconds = time.tv_sec;
    tm *timeinfo = localtime(&seconds);

    // Format date string
    char date[50];
    strftime(date, 50, "%a %b %d %H:%M:%S %Z %Y", timeinfo);

    return date;
}

std::string Session::executeWhoami() const {
    return username();
}

std::string Session::executeW() const {
    return _server.loggedUsers();
}

std::string Session::executeLogout() {
    _state.username.reset();
    return "";
}

void Session::ValidatePathArg(const std::vector<std::string> &args) {
    if (args.size() < 2) {
        throw GrassException("Missing argument");
    }
    if (args[1].length() > MAX_PATH_LENGTH) {
        throw GrassException("The path is too long");
    }
}

} // namespace grass
