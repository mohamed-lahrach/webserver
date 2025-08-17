#include "Server_setup/server.hpp"
#include "config/Lexer.hpp"
#include "config/parser.hpp"
#include <vector>

int	main(int argc, char **argv)
{
	if (argc != 2)
	{
		std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
		std::cerr << "Example: " << argv[0] << " ./test_configs/default.conf" << std::endl;
		return (1);
	}
	
	Lexer lexer(argv[1]);
	std::vector<ServerContext> servers_config;
    std::vector<Token> tokens = lexer.tokenizeAll();
    std::cout << "-------------------------" << std::endl;
    Parser parser(tokens);
    try
    {
        parser.parse();
		servers_config = parser.getServers();
        std::cout << "Parsing completed successfully!" << std::endl;
    }
    catch (const std::runtime_error &e)
    {
        std::cerr << e.what() << std::endl;
        return 1;
    }
	exit(0);
	Server server;
	try
	{
		if (servers_config.empty())
		{
			std::cerr << "No server blocks found in config." << std::endl;
			return 1;
		}
		server.init_data(servers_config[0]);
		server.run(servers_config[0]);
	}
	catch (const std::exception &e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		return (-1);
	}
	return (0);
}
