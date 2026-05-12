#include "ServerConfig.hpp"

ServerConfig::ServerConfig()
	: host("0.0.0.0"),        // bind to all interfaces by default
	  port(0),                 // 0 == "not set", validator rejects this
	  server_names(),
	  client_max_body_size(1048576L),  // 1 MiB default
	  error_pages(),
	  locations()
{
}

ServerConfig::~ServerConfig() {}
