#include <array>
#include <iostream>
#include <signal.h>

#include "grass/server_manager.h"

// Main function for the GRASS server
int main() {
    // Avoid being killed by SIGPIPE if we still haven't replied to a client and
    // the socket is closed
    signal(SIGPIPE, SIG_IGN);

    grass::ServerManager server("grass.conf");
    server.run();
}
