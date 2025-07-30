#include "Server_setup/server.hpp"

int	main(void)
{
	int	PORT;
	Server server;

	PORT = 2440;
<<<<<<< HEAD
	std::string hostname = "127.0.0.1";
=======
	std::string hostname = "127.100.100.1";
>>>>>>> 35ba3d0 (Refactor server and client code: update socket handling, improve error reporting, and adjust function signatures for clarity)
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
