/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigParser.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/12 16:08:40 by gansari           #+#    #+#             */
/*   Updated: 2026/05/12 16:08:42 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ConfigParser.hpp"
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cctype>
#include <set>

ConfigParser::ConfigParser() : _tokens(), _pos(0) {}
ConfigParser::~ConfigParser() {}

// Render a token in error messages. For WORD tokens we show the value;
// for structural tokens we show their symbol so "got ''" never happens.
static std::string	tok_display(const Token& t)
{
	switch (t.type)
	{
		case Token::WORD:   return t.value;
		case Token::LBRACE: return "{";
		case Token::RBRACE: return "}";
		case Token::SEMI:   return ";";
		case Token::END:    return "end of file";
	}
	return "";
}

// ===== Public entry points =====

std::vector<ServerConfig>	ConfigParser::parse_file(const std::string& path)
{
	std::ifstream file(path.c_str());
	if (!file.is_open())
		throw std::runtime_error("cannot open config file: " + path);

	// Read the whole file into a string. For config files this is fine —
	// they're tiny. We wouldn't do this for arbitrary user uploads.
	std::stringstream buf;
	buf << file.rdbuf();
	return parse_string(buf.str());
}

std::vector<ServerConfig>	ConfigParser::parse_string(const std::string& input)
{
	Tokenizer tokenizer;
	_tokens = tokenizer.tokenize(input);
	_pos = 0;

	std::vector<ServerConfig> servers;

	// Top-level: zero or more `server { ... }` blocks.
	while (!match(Token::END))
	{
		if (peek().type != Token::WORD || peek().value != "server")
			error("expected 'server' block at top level, got '"
				+ tok_display(peek()) + "'", peek().line);
		consume();  // eat "server"
		servers.push_back(parse_server());
	}

	if (servers.empty())
		throw std::runtime_error("config file contains no server blocks");

	validate(servers);
	return servers;
}

// ===== Token cursor =====

const Token&	ConfigParser::peek() const
{
	return _tokens[_pos];
}

const Token&	ConfigParser::consume()
{
	const Token& t = _tokens[_pos];
	// Don't advance past END — keeps peek() safe even if a buggy
	// grammar rule tries to consume too much.
	if (t.type != Token::END)
		++_pos;
	return t;
}

bool	ConfigParser::match(Token::Type t) const
{
	return _tokens[_pos].type == t;
}

void	ConfigParser::expect(Token::Type t, const std::string& what)
{
	if (!match(t))
		error("expected " + what + ", got '" + tok_display(peek()) + "'", peek().line);
	consume();
}

std::string	ConfigParser::expect_word(const std::string& what)
{
	if (!match(Token::WORD))
		error("expected " + what + ", got '" + tok_display(peek()) + "'", peek().line);
	return consume().value;
}

void	ConfigParser::expect_semi(const std::string& directive)
{
	if (!match(Token::SEMI))
		error("missing ';' after '" + directive + "' directive", peek().line);
	consume();
}

// ===== Grammar: server block =====

ServerConfig	ConfigParser::parse_server()
{
	ServerConfig srv;
	expect(Token::LBRACE, "'{' after 'server'");

	while (!match(Token::RBRACE) && !match(Token::END))
		parse_server_directive(srv);

	expect(Token::RBRACE, "'}' to close 'server' block");
	return srv;
}

void	ConfigParser::parse_server_directive(ServerConfig& srv)
{
	if (!match(Token::WORD))
		error("expected directive name, got '" + tok_display(peek()) + "'",
			peek().line);

	const std::string name = consume().value;

	// Dispatch on directive name. This is a chain of if/else rather than
	// a map<string, function-ptr> because (a) C++98 doesn't have lambdas,
	// (b) member function pointers are syntactically ugly, and (c) the
	// list is short enough that the linear scan is irrelevant.
	if (name == "listen")
		parse_listen(srv);
	else if (name == "host")
		parse_host(srv);
	else if (name == "server_name")
		parse_server_name(srv);
	else if (name == "client_max_body_size")
	{
		parse_body_size(srv.client_max_body_size);
		expect_semi("client_max_body_size");
	}
	else if (name == "error_page")
		parse_error_page(srv);
	else if (name == "location")
		srv.locations.push_back(parse_location());
	else
		error("unknown directive '" + name + "' in server block", peek().line);
}

// ===== Grammar: location block =====

LocationConfig	ConfigParser::parse_location()
{
	LocationConfig loc;

	loc.path = expect_word("location path");
	if (loc.path.empty() || loc.path[0] != '/')
		error("location path must start with '/', got '" + loc.path + "'",
			peek().line);

	expect(Token::LBRACE, "'{' after location path");
	while (!match(Token::RBRACE) && !match(Token::END))
		parse_location_directive(loc);
	expect(Token::RBRACE, "'}' to close location block");

	return loc;
}

void	ConfigParser::parse_location_directive(LocationConfig& loc)
{
	if (!match(Token::WORD))
		error("expected directive name, got '" + tok_display(peek()) + "'",
			peek().line);

	const std::string name = consume().value;

	if (name == "root")
	{
		loc.root = expect_word("path after 'root'");
		expect_semi("root");
	}
	else if (name == "index")
	{
		loc.index = expect_word("filename after 'index'");
		expect_semi("index");
	}
	else if (name == "methods")
		parse_methods(loc);
	else if (name == "return")
		parse_return(loc);
	else if (name == "autoindex")
		parse_autoindex(loc);
	else if (name == "client_max_body_size")
	{
		parse_body_size(loc.client_max_body_size);
		expect_semi("client_max_body_size");
	}
	else if (name == "upload_store")
	{
		loc.upload_store = expect_word("path after 'upload_store'");
		expect_semi("upload_store");
	}
	else if (name == "cgi_extension")
		parse_cgi(loc);
	else
		error("unknown directive '" + name + "' in location block",
			peek().line);
}

// ===== Per-directive value parsers =====

// listen 8080;     -> port = 8080
// listen 0.0.0.0:8080;  -> host = 0.0.0.0, port = 8080
void	ConfigParser::parse_listen(ServerConfig& srv)
{
	const std::string val = expect_word("port or host:port after 'listen'");
	int line = _tokens[_pos - 1].line;

	std::string host_part;
	std::string port_part;

	size_t colon = val.find(':');
	if (colon == std::string::npos)
	{
		port_part = val;
	}
	else
	{
		host_part = val.substr(0, colon);
		port_part = val.substr(colon + 1);
		if (host_part.empty() || port_part.empty())
			error("invalid 'listen' value '" + val + "'", line);
		srv.host = host_part;
	}

	int port;
	if (!parse_int(port_part, port))
		error("invalid port number '" + port_part + "'", line);
	if (port < 1 || port > 65535)
		error("port out of range (1-65535): " + port_part, line);
	srv.port = port;

	expect_semi("listen");
}

void	ConfigParser::parse_host(ServerConfig& srv)
{
	srv.host = expect_word("host address after 'host'");
	expect_semi("host");
}

// server_name accepts one or more names: `server_name a.com b.com c.com;`
void	ConfigParser::parse_server_name(ServerConfig& srv)
{
	if (!match(Token::WORD))
		error("expected at least one name after 'server_name'", peek().line);
	while (match(Token::WORD))
		srv.server_names.push_back(consume().value);
	expect_semi("server_name");
}

// Accepts plain bytes ("1048576") or suffixed ("1m", "1M", "10k", "10K").
// Note: does NOT consume the trailing ';' — caller does, so this helper
// is reusable for both server-level and location-level directives.
void	ConfigParser::parse_body_size(long& target)
{
	const std::string val = expect_word("size after 'client_max_body_size'");
	int line = _tokens[_pos - 1].line;

	long bytes;
	if (!parse_size(val, bytes))
		error("invalid size '" + val + "'", line);
	if (bytes < 0)
		error("size cannot be negative: " + val, line);
	target = bytes;
}

// error_page 404 /errors/404.html;
// error_page 500 502 503 /errors/5xx.html;   <-- multiple codes, one page
void	ConfigParser::parse_error_page(ServerConfig& srv)
{
	std::vector<int> codes;

	// We need at least one code AND one path. Grammar: WORD+ WORD ';'
	// We collect all WORDs, then split: last one is the path, rest are codes.
	std::vector<std::string> words;
	while (match(Token::WORD))
		words.push_back(consume().value);

	if (words.size() < 2)
		error("'error_page' needs at least one status code and a path",
			peek().line);

	std::string path = words.back();
	for (size_t i = 0; i < words.size() - 1; ++i)
	{
		int code;
		if (!parse_int(words[i], code))
			error("invalid status code '" + words[i] + "'", peek().line);
		if (code < 300 || code > 599)
			error("error_page code out of range (300-599): " + words[i],
				peek().line);
		srv.error_pages[code] = path;
	}

	expect_semi("error_page");
}

void	ConfigParser::parse_methods(LocationConfig& loc)
{
	if (!match(Token::WORD))
		error("expected at least one method after 'methods'", peek().line);

	// Only allow methods we actually implement. Catching typos like
	// `methods GTE POST;` here is much friendlier than 405s at runtime.
	std::set<std::string> allowed;
	allowed.insert("GET");
	allowed.insert("POST");
	allowed.insert("DELETE");

	while (match(Token::WORD))
	{
		std::string m = consume().value;
		if (allowed.find(m) == allowed.end())
			error("unsupported method '" + m
				+ "' (allowed: GET, POST, DELETE)",
				_tokens[_pos - 1].line);
		loc.methods.push_back(m);
	}
	expect_semi("methods");
}

// return 301 /new-url;
void	ConfigParser::parse_return(LocationConfig& loc)
{
	const std::string code_str = expect_word("status code after 'return'");
	int line = _tokens[_pos - 1].line;

	int code;
	if (!parse_int(code_str, code))
		error("invalid redirect code '" + code_str + "'", line);
	if (code < 300 || code > 399)
		error("redirect code must be 3xx, got " + code_str, line);

	loc.has_redirect = true;
	loc.redirect_code = code;
	loc.redirect_url = expect_word("URL after redirect code");
	expect_semi("return");
}

// autoindex on;  /  autoindex off;
void	ConfigParser::parse_autoindex(LocationConfig& loc)
{
	const std::string val = expect_word("'on' or 'off' after 'autoindex'");
	int line = _tokens[_pos - 1].line;

	if (val == "on")
		loc.autoindex = true;
	else if (val == "off")
		loc.autoindex = false;
	else
		error("autoindex expects 'on' or 'off', got '" + val + "'", line);
	expect_semi("autoindex");
}

// cgi_extension .py /usr/bin/python3;
void	ConfigParser::parse_cgi(LocationConfig& loc)
{
	const std::string ext = expect_word("extension after 'cgi_extension'");
	int line = _tokens[_pos - 1].line;

	if (ext.empty() || ext[0] != '.')
		error("CGI extension must start with '.', got '" + ext + "'", line);

	const std::string interp = expect_word("interpreter path after extension");
	loc.cgi_handlers[ext] = interp;
	expect_semi("cgi_extension");
}

// ===== Utilities =====

void	ConfigParser::error(const std::string& msg, int line) const
{
	std::stringstream ss;
	ss << "config error (line " << line << "): " << msg;
	throw std::runtime_error(ss.str());
}

// Strict integer parser. Rejects empty, leading +/-, trailing garbage,
// and values that don't fit in int.
bool	ConfigParser::parse_int(const std::string& s, int& out) const
{
	if (s.empty())
		return false;
	for (size_t i = 0; i < s.size(); ++i)
	{
		if (!std::isdigit(static_cast<unsigned char>(s[i])))
			return false;
	}

	// std::strtol does the overflow check for us. We use it with errno
	// in normal code, but here we double-check by parsing the result.
	char* endptr;
	long v = std::strtol(s.c_str(), &endptr, 10);
	if (*endptr != '\0')
		return false;
	// int range check — on most platforms int is 32-bit, but be safe.
	if (v < -2147483647L - 1 || v > 2147483647L)
		return false;
	out = static_cast<int>(v);
	return true;
}

// Parse "1024", "1k", "1K", "10m", "10M". No spaces between number and suffix.
bool	ConfigParser::parse_size(const std::string& s, long& out) const
{
	if (s.empty())
		return false;

	// Find where the digits end.
	size_t i = 0;
	while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i])))
		++i;
	if (i == 0)
		return false;

	std::string num_part = s.substr(0, i);
	std::string suffix = s.substr(i);

	char* endptr;
	long v = std::strtol(num_part.c_str(), &endptr, 10);
	if (*endptr != '\0')
		return false;

	long multiplier = 1;
	if (suffix.empty())
		multiplier = 1;
	else if (suffix == "k" || suffix == "K")
		multiplier = 1024L;
	else if (suffix == "m" || suffix == "M")
		multiplier = 1024L * 1024L;
	else if (suffix == "g" || suffix == "G")
		multiplier = 1024L * 1024L * 1024L;
	else
		return false;

	// Cheap overflow check: if v > LONG_MAX / multiplier, multiplying overflows.
	if (multiplier > 1 && v > (2147483647L / multiplier))
		return false;

	out = v * multiplier;
	return true;
}

// ===== Semantic validation =====

void	ConfigParser::validate(const std::vector<ServerConfig>& servers)
{
	for (size_t i = 0; i < servers.size(); ++i)
		validate_server(servers[i]);

	// Cross-server check: two servers can't bind the same host:port,
	// unless we implement virtual hosting (which the subject marks
	// as out of scope). For now, reject duplicates.
	for (size_t i = 0; i < servers.size(); ++i)
	{
		for (size_t j = i + 1; j < servers.size(); ++j)
		{
			if (servers[i].host == servers[j].host
				&& servers[i].port == servers[j].port)
			{
				std::stringstream ss;
				ss << "duplicate listen " << servers[i].host
					<< ":" << servers[i].port
					<< " — two server blocks bind the same address";
				throw std::runtime_error(ss.str());
			}
		}
	}
}

void	ConfigParser::validate_server(const ServerConfig& srv)
{
	if (srv.port == 0)
		throw std::runtime_error(
			"server block is missing 'listen' directive");

	std::stringstream ctx;
	ctx << srv.host << ":" << srv.port;

	// Detect duplicate location paths within a server.
	std::set<std::string> paths;
	for (size_t i = 0; i < srv.locations.size(); ++i)
	{
		const std::string& p = srv.locations[i].path;
		if (paths.find(p) != paths.end())
			throw std::runtime_error("duplicate location '" + p
				+ "' in server " + ctx.str());
		paths.insert(p);
		validate_location(srv.locations[i], ctx.str());
	}
}

void	ConfigParser::validate_location(const LocationConfig& loc,
										 const std::string& server_ctx)
{
	// A location that isn't a pure redirect needs a root to serve from.
	if (!loc.has_redirect && loc.root.empty())
		throw std::runtime_error("location '" + loc.path
			+ "' in server " + server_ctx
			+ " has no 'root' and no 'return' — nothing to serve");

	// If uploads are allowed, the location must accept POST.
	if (!loc.upload_store.empty())
	{
		bool has_post = false;
		for (size_t i = 0; i < loc.methods.size(); ++i)
			if (loc.methods[i] == "POST")
				has_post = true;
		if (!has_post)
			throw std::runtime_error("location '" + loc.path
				+ "' in server " + server_ctx
				+ " sets 'upload_store' but doesn't allow POST");
	}
}
