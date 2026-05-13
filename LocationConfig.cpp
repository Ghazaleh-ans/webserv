#include "LocationConfig.hpp"

LocationConfig::LocationConfig()
	: path(""),
	  root(""),
	  index(""),
	  methods(),
	  has_redirect(false),
	  redirect_code(0),
	  redirect_url(""),
	  autoindex(false),
	  client_max_body_size(-1), // -1 == "inherit from server"
	  upload_store(""),
	  cgi_handlers()
{
}

LocationConfig::~LocationConfig() {}
