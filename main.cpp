#include "Server_setup/server.hpp"

int	main(void)
{
	int	PORT;
	Server server;

	PORT = 2440;
	std::string hostname = "127.0.0.1";
	try
	{
		server.init_data(PORT, hostname);
		server.run();
	}
	catch (const std::exception &e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		return (-1);
	}
	return (0);
}
