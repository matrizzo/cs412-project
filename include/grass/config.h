#pragma once

#include <cstdint>
#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <variant>

namespace grass {

/*
 * The config class contains the configuration of a GRASS server. It contains
 * the parser for configuration files
 */
class Config {
public:
    // Constructs the server's configuration manually, useful for testing
    Config(const std::map<std::string, std::string> &credential,
        uint16_t portNumber,
        const std::string &dir);

    // Returns the port number on which the server should listen
    uint16_t portNumber() const;

    // Returns the path of the base directory of the server
    const std::string &directory() const;

    // Checks a username/password pair to see if they are valid
    bool checkLogin(const std::string &username,
        const std::string &password) const;

    /*
     * Factory method that parses the configuration of the server from a
     * file
     */
    static Config parseFromFile(const std::string &filename);

    // Removes comments from a configuration line
    static std::string removeComments(std::string &line);

private:
    // Login credentials for the server (username, password)
    const std::map<std::string, std::string> _credentials;

    // The port number on which the server should listen
    const uint16_t _portNumber;

    // The base directory of the server
    const std::string _directory;
};

} // namespace grass
