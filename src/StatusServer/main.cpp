/**
 * StatusServer
 */

#include <exception>
#include <iostream>

#include "StatusServer.h"

int main()
{
    try {
        StatusServer server;
        server.start();
        return 0;
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    return 0;
}
