#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstddef>   // size_t

namespace extcfg
{

// just the data the parser fills in — deliberately no parsing logic in here.
//   interface:port pairs              -> ListenSpec, ServerConfig::listens
//   default error pages               -> ServerConfig::error_pages
//   max client body size              -> ServerConfig::client_max_body_size
//   accepted HTTP methods per route   -> LocationConfig::allowed_methods
//   HTTP redirection per route        -> LocationConfig::redirect
//   root per route (the /kapouet one) -> LocationConfig::root
//   directory listing on/off          -> LocationConfig::autoindex
//   default file for a directory      -> LocationConfig::index
//   where uploads get stored          -> LocationConfig::upload_store
//   CGI by file extension             -> LocationConfig::cgi

// one "listen" directive: an interface + a port.
//   listen 8080;              -> host = "0.0.0.0", port = 8080
//   listen 127.0.0.1:8080;    -> host = "127.0.0.1", port = 8080
struct ListenSpec {
	std::string host;
	int         port;
	bool        host_explicit;   // true when written as host:port, so a later
	                             // standalone `host` directive won't override it

	ListenSpec();
};

// optional per-location redirect. `enabled` is flipped true by a `return` directive.
struct Redirect {
	int         code;
	std::string target;
	bool        enabled;

	Redirect();
};

// everything that can live inside a `location` block.
struct LocationConfig {
	std::string                         path;
	std::vector<std::string>            allowed_methods;
	std::string                         root;
	std::string                         index;
	bool                                autoindex;
	Redirect                            redirect;
	std::string                         upload_store;
	std::map<std::string, std::string>  cgi;   // ".py" -> "/usr/bin/python3"
	bool                                has_client_max_body_size;  // per-location override
	std::size_t                         client_max_body_size;

	LocationConfig();
};

// one `server { ... }` block from the file.
struct ServerConfig {
	std::vector<ListenSpec>           listens;
	std::string                       server_name;     // space-joined if several
	std::string                       host_override;   // from a standalone `host` directive
	std::string                       root;
	std::size_t                       client_max_body_size;
	std::map<int, std::string>        error_pages;     // 404 -> "/404.html"
	std::vector<LocationConfig>       locations;

	ServerConfig();
};

// the top of the tree: a list of server blocks plus the calls to build and read it.
class Config {
	private:
		std::vector<ServerConfig> servers_;

		void validate();

		Config(const Config&);
		Config& operator=(const Config&);

	public:
		Config();

		// read `path`, then lexer -> parser -> validator. throws
		// ConfigException the moment anything along that chain goes wrong.
		void load(const std::string& path);

		const std::vector<ServerConfig>& servers() const;
};

} // namespace extcfg
