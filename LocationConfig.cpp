#include "LocationConfig.hpp"

// Defaults chosen to be safe ("does nothing surprising") rather than useful.
// A location with no `methods` directive should accept only GET — but we
// represent that with an empty vector here and apply the default when the
// router checks methods. That way "user explicitly set methods" and
// "user said nothing" are distinguishable.
LocationConfig::LocationConfig()
	: path(""),
	  root(""),
	  index(""),
	  methods(),
	  has_redirect(false),
	  redirect_code(0),
	  redirect_url(""),
	  autoindex(false),
	  client_max_body_size(-1),  // -1 == "inherit from server"
	  upload_store(""),
	  cgi_handlers()
{
}

LocationConfig::~LocationConfig() {}
