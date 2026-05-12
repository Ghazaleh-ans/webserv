#include <iostream>
#include <iomanip>
#include "ConfigParser.hpp"

// Pretty-printer for the parsed config. This is the proof that parsing
// worked: if we can re-emit a human-readable form of what we parsed,
// we trust the result.
static void	print_location(const LocationConfig& loc, int indent)
{
	std::string pad(indent, ' ');
	std::cout << pad << "location " << loc.path << " {\n";
	if (!loc.root.empty())
		std::cout << pad << "  root: " << loc.root << "\n";
	if (!loc.index.empty())
		std::cout << pad << "  index: " << loc.index << "\n";
	if (!loc.methods.empty())
	{
		std::cout << pad << "  methods:";
		for (size_t i = 0; i < loc.methods.size(); ++i)
			std::cout << " " << loc.methods[i];
		std::cout << "\n";
	}
	if (loc.has_redirect)
		std::cout << pad << "  return: " << loc.redirect_code
			<< " " << loc.redirect_url << "\n";
	std::cout << pad << "  autoindex: "
		<< (loc.autoindex ? "on" : "off") << "\n";
	if (loc.client_max_body_size >= 0)
		std::cout << pad << "  client_max_body_size: "
			<< loc.client_max_body_size << "\n";
	if (!loc.upload_store.empty())
		std::cout << pad << "  upload_store: " << loc.upload_store << "\n";
	for (std::map<std::string, std::string>::const_iterator it
			= loc.cgi_handlers.begin(); it != loc.cgi_handlers.end(); ++it)
		std::cout << pad << "  cgi: " << it->first
			<< " -> " << it->second << "\n";
	std::cout << pad << "}\n";
}

static void	print_server(const ServerConfig& srv)
{
	std::cout << "server {\n";
	std::cout << "  listen: " << srv.host << ":" << srv.port << "\n";
	if (!srv.server_names.empty())
	{
		std::cout << "  server_name:";
		for (size_t i = 0; i < srv.server_names.size(); ++i)
			std::cout << " " << srv.server_names[i];
		std::cout << "\n";
	}
	std::cout << "  client_max_body_size: "
		<< srv.client_max_body_size << "\n";
	for (std::map<int, std::string>::const_iterator it
			= srv.error_pages.begin(); it != srv.error_pages.end(); ++it)
		std::cout << "  error_page: " << it->first
			<< " -> " << it->second << "\n";
	for (size_t i = 0; i < srv.locations.size(); ++i)
		print_location(srv.locations[i], 2);
	std::cout << "}\n";
}

int	main(int argc, char** argv)
{
	if (argc != 2)
	{
		std::cerr << "usage: " << argv[0] << " <config-file>\n";
		return 1;
	}

	try
	{
		ConfigParser parser;
		std::vector<ServerConfig> servers = parser.parse_file(argv[1]);

		std::cout << "=== Parsed " << servers.size()
			<< " server block(s) ===\n\n";
		for (size_t i = 0; i < servers.size(); ++i)
			print_server(servers[i]);

		return 0;
	}
	catch (const std::exception& e)
	{
		std::cerr << "ERROR: " << e.what() << "\n";
		return 1;
	}
}
