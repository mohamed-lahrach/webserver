#include "Server_setup/server.hpp"

int main()
{
    int PORT = 2440;
    std::string hostname= "server";
    try
    {
        Server server;
        server.init_data(PORT, hostname);
        server.run();
    }
    catch(const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    }
    return 0;
}
