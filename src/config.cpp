#include <exception>
#include <fstream>
#include <map>

#include "../include/grass/config.h"
#include "../include/grass/grass.h"

namespace grass {
Config::Config(const std::map<std::string, std::string> &credentials,
    uint16_t pnumber,
    const std::string &dir)
    : _credentials(credentials), _portNumber(pnumber), _directory(dir) {}

bool Config::checkLogin(const std::string &username,
    const std::string &password) const {

    // Checking the existence of the username
    if (this->_credentials.find(username) != this->_credentials.end()) {
        std::string passwordOfUser = this->_credentials.at(username);
        return passwordOfUser == password;
    } else {
        return false;
    }
}

std::string Config::removeComments(std::string &line) {
    // Getting rid of the comments
    std::size_t index = line.find('#');
    if (index != std::string::npos)
        line = line.substr(0, index); // Line is now free of comments

    return line;
}

uint16_t Config::portNumber() const { return this->_portNumber; }

const std::string &Config::directory() const { return this->_directory; }

Config Config::parseFromFile(const std::string &filename) {
    std::ifstream infile(filename);

    std::string directory = "";
    std::map<std::string, std::string> credentials;
    int port = -1;

    for (std::string line; getline(infile, line);) {

        std::size_t index = 1;
        line = removeComments(line);
        // Checking which type of command it is

        // If it is the directory command
        index = line.find("base ");

        // Parsing the directory
        if (index == 0)
            directory = line.substr(5);

        // Taking the port
        index = line.find("port ");
        if (index == 0) {
            try {
                port = std::stoi(line.substr(5));
            } catch (...) {
                port = -1;
            }
        }

        // Checking if it is for the credentials
        index = line.find("user ");

        if (index == 0) {
            line = line.substr(5); // Only credentials remaining

            // Separating username and credential
            index = line.find(" ");
            if (!(index == 0 ||
                    index == line.length())) { // There is a separation
                std::string username = line.substr(0, index);
                std::string password = line.substr(index + 1);

                // Checking that there is no more spaces in the line
                if (password.find(" ") == std::string::npos)
                    credentials[username] = password;
            }
        }
    }

    // Checking that the port number finishes in the correct range
    if (port > 65535 || port < 0) {
        throw GrassException("Port number out of range");
    }

    // Check that directory has been initialized
    if (directory == "") {
        throw GrassException(
            "Invalid config file: base directory should be specified");
    }

    return Config(credentials, static_cast<uint16_t>(port), directory);
}
} // namespace grass
