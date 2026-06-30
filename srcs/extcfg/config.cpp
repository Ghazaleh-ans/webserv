#include "extcfg/config.hpp"
#include "extcfg/parser.hpp"
#include "extcfg/lexer.hpp"
#include "extcfg/exceptions.hpp"
#include "extcfg/utils.hpp"
#include <sstream>
#include <map>

// the ctors for the config structs, plus Config::load glue and validate().

namespace extcfg
{

ListenSpec::ListenSpec()
	: host("0.0.0.0")   // just a port given? bind every interface
	, port(0)           // 0 = "nobody set this"; the validator rejects it later
	, host_explicit(false)
{}

Redirect::Redirect()
	: code(0)
	, target("")
	, enabled(false)    // off until a `return` directive turns it on
{}

LocationConfig::LocationConfig()
	: path("")
	, allowed_methods()       // empty == allow everything
	, root("")                // empty == inherit ServerConfig::root
	, index("")
	, autoindex(false)
	, redirect()
	, upload_store("")
	, cgi()
	, has_client_max_body_size(false)   // false == inherit the server limit
	, client_max_body_size(0)
{}

ServerConfig::ServerConfig()
	: listens()
	, server_name("")
	, host_override("")
	, root("")
	, client_max_body_size(1024 * 1024)   // 1 MiB unless the config overrides it
	, error_pages()
	, locations()
{}

Config::Config()
	: servers_()
{}

const std::vector<ServerConfig>& Config::servers() const {
	return servers_;
}

static std::string readFile(const std::string& path) {
	std::string source;
	if (!readFileToString(path, source))
		throw ConfigException("cannot open config file '" + path + "'");
	return source;
}

void Config::load(const std::string& path) {
	std::string source = readFile(path);
	Lexer       lex(source, path);
	Parser      parser(lex);
	parser.parseFile(servers_);
	validate();
}

// the checks the grammar can't catch on its own:
//   1. there has to be at least one server block.
//   2. every server needs at least one `listen`.
//   3. no two servers can claim the same (host, port).
void Config::validate() {
	if (servers_.empty())
		throw ConfigException("config: no server blocks defined");

	std::map<std::string, std::size_t> seen_listens;

	for (std::size_t i = 0; i < servers_.size(); ++i) {
		const ServerConfig& s = servers_[i];

		if (s.listens.empty()) {
			std::stringstream msg;
			msg << "config: server[" << i << "] has no 'listen' directive";
			throw ConfigException(msg.str());
		}

		for (std::size_t j = 0; j < s.listens.size(); ++j) {
			const ListenSpec& ls = s.listens[j];

			std::stringstream key_ss;
			key_ss << ls.host << ":" << ls.port;
			const std::string key = key_ss.str();

			std::map<std::string, std::size_t>::iterator it
				= seen_listens.find(key);
			if (it != seen_listens.end()) {
				std::stringstream msg;
				msg << "config: duplicate listen " << key
					<< " (server[" << i << "] conflicts with server["
					<< it->second << "])";
				throw ConfigException(msg.str());
			}
			seen_listens[key] = i;
		}
	}
}

} // namespace extcfg
