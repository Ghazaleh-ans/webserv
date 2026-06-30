/* ************************************************************************** */
/*                                                                            */
/*   ConfigAdapter.cpp                                                        */
/*                                                                            */
/*   Converts extcfg::ServerConfig (the imported parser's output) into this   */
/*   project's ServerConfig/LocationConfig that the rest of the server reads. */
/*                                                                            */
/* ************************************************************************** */

#include "config/ConfigAdapter.hpp"
#include "extcfg/config.hpp"

#include <climits>
#include <sstream>
#include <stdexcept>

namespace ConfigAdapter
{

// extcfg stores body size as size_t; this project's structs use a signed long
// (with -1 meaning "inherit" at the location level). Clamp so an absurdly large
// configured size can't wrap negative.
static long	to_signed_size(std::size_t v)
{
	if (v > static_cast<std::size_t>(LONG_MAX))
		return LONG_MAX;
	return static_cast<long>(v);
}

// Translate one extcfg location into this project's LocationConfig.
// `server_root` is pushed down as the default when the location omits its own
// root (extcfg allows a server-level root; this project's structs don't have
// one, so locations must each carry a concrete root).
static LocationConfig	convert_location(const extcfg::LocationConfig& src,
									const std::string& server_root)
{
	LocationConfig dst;

	dst.path = src.path;
	dst.root = src.root.empty() ? server_root : src.root;
	dst.index = src.index;
	dst.methods = src.allowed_methods;

	dst.has_redirect = src.redirect.enabled;
	dst.redirect_code = src.redirect.code;
	dst.redirect_url = src.redirect.target;

	dst.autoindex = src.autoindex;

	// extcfg doesn't capture a per-location body-size override -> inherit.
	dst.client_max_body_size = -1;

	// per-location body-size override (-1 means "inherit the server limit")
	dst.client_max_body_size = src.has_client_max_body_size
		? to_signed_size(src.client_max_body_size) : -1;

	dst.upload_store = src.upload_store;
	dst.cgi_handlers = src.cgi;

	return dst;
}

// extcfg stores server_name space-joined; split it back into the vector this
// project's ServerConfig carries.
static std::vector<std::string>	split_names(const std::string& joined)
{
	std::vector<std::string> names;
	std::istringstream iss(joined);
	std::string n;
	while (iss >> n)
		names.push_back(n);
	return names;
}

// The per-location semantic checks this project enforced before the swap, so
// behaviour stays identical: a non-redirect location needs a root, and an
// upload location must accept POST.
static void	validate_location(const LocationConfig& loc,
								const std::string& ctx)
{
	if (!loc.has_redirect && loc.root.empty())
		throw std::runtime_error("location '" + loc.path + "' in server " + ctx
			+ " has no 'root' and no 'return' - nothing to serve");

	if (!loc.upload_store.empty())
	{
		bool has_post = false;
		for (size_t i = 0; i < loc.methods.size(); ++i)
			if (loc.methods[i] == "POST")
				has_post = true;
		if (!has_post)
			throw std::runtime_error("location '" + loc.path + "' in server " + ctx
				+ " sets 'upload_store' but doesn't allow POST");
	}
}

std::vector<ServerConfig>	load(const std::string& path)
{
	extcfg::Config cfg;
	cfg.load(path); // throws extcfg::ConfigException (a std::exception) on failure

	std::vector<ServerConfig> out;

	const std::vector<extcfg::ServerConfig>& parsed = cfg.servers();
	for (size_t i = 0; i < parsed.size(); ++i)
	{
		const extcfg::ServerConfig& src = parsed[i];

		// extcfg allows several `listen` directives per server block; this
		// project models one host:port per ServerConfig (one Listener each),
		// so expand each listen into its own ServerConfig sharing the rest.
		for (size_t j = 0; j < src.listens.size(); ++j)
		{
			ServerConfig dst;
			dst.host = src.listens[j].host;
			dst.port = src.listens[j].port;

			dst.server_names = split_names(src.server_name);

			dst.client_max_body_size = to_signed_size(src.client_max_body_size);
			dst.error_pages = src.error_pages;

			std::stringstream ctx_ss;
			ctx_ss << dst.host << ":" << dst.port;
			const std::string ctx = ctx_ss.str();

			for (size_t k = 0; k < src.locations.size(); ++k)
			{
				LocationConfig loc = convert_location(src.locations[k], src.root);
				validate_location(loc, ctx);
				dst.locations.push_back(loc);
			}

			out.push_back(dst);
		}
	}

	return out;
}

} // namespace ConfigAdapter
