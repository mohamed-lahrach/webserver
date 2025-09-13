#include "Server_setup/server.hpp"
#include "config/Lexer.hpp"
#include "config/parser.hpp"
#include <vector>
#include <signal.h>

int	main(int argc, char **argv)
{    
	
    signal(SIGPIPE, SIG_IGN);

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
        std::cout << "Found " << servers_config.size() << " server configurations" << std::endl;
    }
    catch (const std::runtime_error &e)
    {
        std::cerr << e.what() << std::endl;
        return 1;
    }
	//exit(0); // Temporary exit to test parsing only
	// Debug: print server info and location details

	Server server;
	try
	{
		if (servers_config.empty())
		{
			std::cerr << "No server blocks found in config." << std::endl;
			return 1;
		}
		server.init_data(servers_config);
		server.run();
	}
	catch (const std::exception &e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		return (-1);
	}
	return (0);
}


