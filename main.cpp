#include "Server_setup/server.hpp"

int main(void)
{

	// parser phase
	Lexer lexer("./test_configs/default.conf");
	std::vector
	std::vector<Token> tokens = lexer.tokenizeAll();
	std::cout << "-------------------------" << std::endl;
	Parser parser(tokens);
	try
	{
		parser.parse();
		parser.getServers();
		std::cout << "Parsing completed successfully!" << std::endl;
	}
	catch (const std::runtime_error &e)
	{
		std::cerr << e.what() << std::endl;
	}

	int PORT;
	Server server;

	PORT = 8080;
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
