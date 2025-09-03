// main.cpp  (C++98)

#include <iostream>
#include <fstream>
#include <vector>
#include <string>

#include "Server_setup/server.hpp"
#include "config/Lexer.hpp"
#include "config/parser.hpp"

// ---- Types (as you provided) ----
// Keep only if not already included by your headers.
// struct LocationContext { ... }
// typedef std::pair<std::vector<int>, std::string> ErrorPagePair;
// struct ServerContext { ... }

static void writeJoinedInts(std::ostream& os, const std::vector<int>& xs)
{
    for (std::vector<int>::const_iterator it = xs.begin(); it != xs.end(); ++it) {
        if (it != xs.begin()) os << ",";
        os << *it;
    }
}

static void writeJoinedStrings(std::ostream& os, const std::vector<std::string>& xs)
{
    for (std::vector<std::string>::const_iterator it = xs.begin(); it != xs.end(); ++it) {
        if (it != xs.begin()) os << " ";
        os << *it;
    }
}

static void saveServerConfigsToFile(const std::vector<ServerContext>& servers_config,
                                    const std::string& filename)
{
    std::ofstream outFile(filename.c_str());
    if (!outFile.is_open()) {
        std::cerr << "Failed to open output file: " << filename << std::endl;
        return;
    }

    for (std::vector<ServerContext>::const_iterator it = servers_config.begin();
         it != servers_config.end(); ++it)
    {
        const ServerContext& server = *it;

        outFile << "ServerContext:" << std::endl;
        outFile << "  Host: " << server.host << std::endl;
        outFile << "  Port: " << server.port << std::endl;
        outFile << "  Root: " << server.root << std::endl;

        outFile << "  Indexes: ";
        writeJoinedStrings(outFile, server.indexes);
        outFile << std::endl;

        outFile << "  Autoindex: " << server.autoindex << std::endl;
        outFile << "  ClientMaxBodySize: " << server.clientMaxBodySize << std::endl;

        outFile << "  ErrorPages:" << std::endl;
        for (std::vector<ErrorPagePair>::const_iterator ep = server.errorPages.begin();
             ep != server.errorPages.end(); ++ep)
        {
            outFile << "    Codes: ";
            writeJoinedInts(outFile, ep->first);
            outFile << " -> File: " << ep->second << std::endl;
        }

        outFile << "  Locations:" << std::endl;
        for (std::vector<LocationContext>::const_iterator locIt = server.locations.begin();
             locIt != server.locations.end(); ++locIt)
        {
            const LocationContext& loc = *locIt;
            outFile << "    Path: " << loc.path << std::endl;
            outFile << "      Root: " << loc.root << std::endl;

            outFile << "      Indexes: ";
            writeJoinedStrings(outFile, loc.indexes);
            outFile << std::endl;

            outFile << "      Autoindex: " << loc.autoindex << std::endl;

            outFile << "      AllowedMethods: ";
            writeJoinedStrings(outFile, loc.allowedMethods);
            outFile << std::endl;

            outFile << "      Return: " << loc.returnDirective << std::endl;
            outFile << "      CGI Extension: " << loc.cgiExtension << std::endl;
            outFile << "      CGI Path: " << loc.cgiPath << std::endl;
            outFile << "      Upload Store: " << loc.uploadStore << std::endl;
        }

        outFile << std::endl;
    }

    outFile.close();
    std::cout << "Parsed server configurations saved to '" << filename << "'." << std::endl;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
        std::cerr << "Example: " << argv[0] << " ./test_configs/default.conf" << std::endl;
        return 1;
    }

    // 1) Lex & parse
    Lexer lexer(argv[1]);
    std::vector<Token> tokens = lexer.tokenizeAll();
    std::cout << "-------------------------" << std::endl;

    Parser parser(tokens);
    std::vector<ServerContext> servers_config;

    try {
        parser.parse();
        servers_config = parser.getServers();
        std::cout << "Parsed " << servers_config.size() << " server blocks." << std::endl;

        // Dump parsed result for inspection
        saveServerConfigsToFile(servers_config, "parsed_servers.txt");
        std::cout << "Parsing completed successfully!" << std::endl;
    }
    catch (const std::runtime_error &e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }
	//exit(0);
    // 2) Start server(s). For now: run the first parsed server block.
    if (servers_config.empty()) {
        std::cerr << "No server blocks found in config." << std::endl;
        return 1;
    }

    Server server;
    try {
        server.init_data(servers_config[0]);
        server.run(servers_config[0]);
    }
    catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}
