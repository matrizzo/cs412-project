#include <iostream>

#include "grass/client_manager.h"
#include "grass/grass.h"

// Main function for the GRASS client
int main(int argc, char *argv[]) {

    // Verify number of arguments
    if (argc != 3 && argc != 5) {
        std::cout << "Usage:\t./client <server-ip> <server-port> [<input-file> "
                     "<output-file>]"
                  << std::endl;
        return 1;
    }

    // Parse port number
    unsigned long port_arg = std::strtoul(argv[2], nullptr, 10);
    if (port_arg > 65535) {
        std::cout << "Port number out of range";
        return -1;
    }

    uint16_t port = static_cast<uint16_t>(port_arg);

    // Create and start client
    grass::ClientManager client =
        (argc == 3) ? grass::ClientManager(argv[1], port)
                    : grass::ClientManager(argv[1], port, argv[3], argv[4]);

    client.run();

    return 0;
}
