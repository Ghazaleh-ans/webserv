#include "extcfg/parser.hpp"
#include "extcfg/exceptions.hpp"
#include "extcfg/utils.hpp"
#include <sstream>
#include <iostream>   // std::cerr — for the unknown-directive warnings
#include <limits>     // std::numeric_limits — the overflow check in parseSize

namespace extcfg
{

// path helpers for the `include` directive. static so they stay local to this file.

static std::string readIncludedFile(const std::string& path) {
	std::string source;
	if (!readFileToString(path, source))
		throw ConfigException("cannot open included file '" + path + "'");
	return source;
}

static std::string dirnameOf(const std::string& path) {
	std::string::size_type slash = path.rfind('/');
	if (slash == std::string::npos) return "";
	return path.substr(0, slash + 1);
}

static std::string joinPath(const std::string& dir, const std::string& name) {
	if (!name.empty() && name[0] == '/') return name;
	return dir + name;
}

// the architecture + grammar overview lives in includes/extcfg/parser.hpp.

Parser::Parser(Lexer& lex)
	: lex_(lex)
	, owned_cycle_guard_()
	, cycle_guard_(&owned_cycle_guard_)
	, base_dir_(dirnameOf(lex.origin()))
{
	cycle_guard_->insert(lex.origin());
}

Parser::Parser(Lexer& lex, std::set<std::string>& shared_cycle_guard,
			   const std::string& base_dir)
	: lex_(lex)
	, owned_cycle_guard_()
	, cycle_guard_(&shared_cycle_guard)
	, base_dir_(base_dir)
{}

bool Parser::match(TokenKind kind) {
	if (lex_.peek().kind != kind)
		return false;
	lex_.next();
	return true;
}

const Token& Parser::expect(TokenKind kind, const char* context) {
	const Token& t = lex_.peek();
	if (t.kind != kind) {
		std::stringstream ss;
		ss << locOf(t) << "expected ";
		switch (kind) {
			case TOK_WORD:   ss << "a word";       break;
			case TOK_LBRACE: ss << "'{'";          break;
			case TOK_RBRACE: ss << "'}'";          break;
			case TOK_SEMI:   ss << "';'";          break;
			case TOK_EOF:    ss << "end of file";  break;
		}
		ss << " " << context << ", got '" << t.text << "'";
		throw ConfigException(ss.str());
	}
	return lex_.next();
}

std::string Parser::locOf(const Token& t) const {
	std::stringstream ss;
	ss << lex_.origin() << ":" << t.line << ": ";
	return ss.str();
}

void Parser::skipToEndOfDirective() {
	// balanced-brace skip used to recover after an unknown directive:
	//   - depth 0, ';'  -> it was a leaf, eat the ';' and I'm done.
	//   - depth 0, '}'  -> closes the block I'm INSIDE, leave it for the caller.
	//   - '{'           -> a body opens, depth++.
	//   - depth >0, '}' -> closes a nested body, depth--.
	//   - depth >0, ';' -> a leaf inside the body I'm skipping, just eat it.
	int depth = 0;
	while (true) {
		const Token& t = lex_.peek();
		if (t.kind == TOK_EOF) return;

		if (t.kind == TOK_RBRACE) {
			if (depth == 0) return;       // not mine — belongs to the enclosing block
			lex_.next();
			--depth;
			if (depth == 0) return;       // just closed this directive's own body
			continue;
		}
		if (t.kind == TOK_LBRACE) {
			lex_.next();
			++depth;
			continue;
		}
		if (t.kind == TOK_SEMI) {
			lex_.next();
			if (depth == 0) return;       // end of a leaf directive
			continue;                     // ';' buried in the body I'm skipping
		}
		lex_.next();                      // anything else: swallow and move on
	}
}

// the grammar itself — one method per production (recursive descent).

void Parser::parseFile(std::vector<ServerConfig>& out) {
	out.clear();
	while (lex_.peek().kind != TOK_EOF) {
		const Token& head = lex_.peek();
		if (head.kind == TOK_WORD && head.text == "server") {
			lex_.next();  // eat "server"
			ServerConfig server;
			parseServerBlock(server);
			// A standalone `host` directive overrides the host of every listen
			// that was written as a bare port (host:port forms keep their host).
			if (!server.host_override.empty()) {
				for (std::size_t i = 0; i < server.listens.size(); ++i)
					if (!server.listens[i].host_explicit)
						server.listens[i].host = server.host_override;
			}
			out.push_back(server);
			continue;
		}
		throw ConfigException(locOf(head)
			+ "expected 'server' block at top level, got '" + head.text + "'");
	}
}

void Parser::parseServerBlock(ServerConfig& server) {
	expect(TOK_LBRACE, "to open server block");
	while (true) {
		const Token& t = lex_.peek();
		if (t.kind == TOK_RBRACE) { lex_.next(); return; }
		if (t.kind == TOK_EOF)
			throw ConfigException(locOf(t) + "unclosed server block, expected '}'");
		parseServerDirective(server);
	}
}

void Parser::parseServerDirective(ServerConfig& server) {
	const Token& name = expect(TOK_WORD, "as directive name");

	if (name.text == "listen")               { parseListen(server);             return; }
	if (name.text == "host")                 { parseHost(server);               return; }
	if (name.text == "server_name")          { parseServerName(server);         return; }
	if (name.text == "root")                 { parseRoot(server);               return; }
	if (name.text == "client_max_body_size") { parseClientMaxBodySize(server);  return; }
	if (name.text == "error_page")           { parseErrorPage(server);          return; }
	if (name.text == "location")             { parseLocation(server);           return; }
	if (name.text == "include")              { parseInclude(server);            return; }

	// be lenient like nginx: warn and skip rather than die. the subject says the
	// config can have extra keys (IV.3), so an unknown directive isn't fatal.
	std::cerr << locOf(name)
			  << "warning: unknown directive '" << name.text
			  << "', ignoring" << std::endl;
	skipToEndOfDirective();
}

void Parser::parseListen(ServerConfig& server) {
	const Token& value = expect(TOK_WORD, "as 'listen' argument");
	expect(TOK_SEMI, "after 'listen' value");

	ListenSpec spec;
	splitHostPort(value, spec.host, spec.port);
	// host:port (or [ipv6]:port) carries an explicit host; a bare port does not.
	spec.host_explicit = (value.text.find(':') != std::string::npos);
	server.listens.push_back(spec);
}

// `host 127.0.0.1;` — a standalone host applied to bare-port listens once the
// whole server block is parsed (see parseFile).
void Parser::parseHost(ServerConfig& server) {
	const Token& value = expect(TOK_WORD, "as 'host' argument");
	expect(TOK_SEMI, "after 'host' value");
	server.host_override = value.text;
}

// `server_name a b c;` — one or more names. Stored space-joined; the adapter
// splits them back out. (server_name isn't used for routing, but we keep them
// all rather than dropping the extras.)
void Parser::parseServerName(ServerConfig& server) {
	if (lex_.peek().kind != TOK_WORD)
		throw ConfigException(locOf(lex_.peek())
			+ "'server_name' needs at least one name");
	std::string names;
	while (lex_.peek().kind == TOK_WORD) {
		if (!names.empty())
			names += " ";
		names += lex_.next().text;
	}
	expect(TOK_SEMI, "after 'server_name' value");
	server.server_name = names;
}

void Parser::parseRoot(ServerConfig& server) {
	const Token& value = expect(TOK_WORD, "as 'root' argument");
	expect(TOK_SEMI, "after 'root' value");
	server.root = value.text;
}

void Parser::parseClientMaxBodySize(ServerConfig& server) {
	const Token& value = expect(TOK_WORD, "as 'client_max_body_size' argument");
	expect(TOK_SEMI, "after 'client_max_body_size' value");
	server.client_max_body_size = parseSize(value);
}

void Parser::parseErrorPage(ServerConfig& server) {
	std::vector<Token> args;
	while (lex_.peek().kind != TOK_SEMI) {
		const Token& t = lex_.peek();
		if (t.kind != TOK_WORD)
			throw ConfigException(locOf(t)
				+ "unexpected token in 'error_page', got '" + t.text + "'");
		args.push_back(lex_.next());
	}
	expect(TOK_SEMI, "after 'error_page' values");

	if (args.size() < 2) {
		const Token& where = args.empty() ? lex_.peek() : args[0];
		throw ConfigException(locOf(where)
			+ "'error_page' needs at least one status code and a path");
	}

	const std::string& path = args.back().text;
	for (std::size_t i = 0; i < args.size() - 1; ++i) {
		int code = parseStatusCode(args[i]);
		server.error_pages[code] = path;
	}
}

void Parser::parseLocation(ServerConfig& server) {
	const Token& pathTok = expect(TOK_WORD, "as 'location' path");
	LocationConfig loc;
	loc.path = pathTok.text;
	parseLocationBlock(loc);
	server.locations.push_back(loc);
}

void Parser::parseLocationBlock(LocationConfig& loc) {
	expect(TOK_LBRACE, "to open location block");
	while (true) {
		const Token& t = lex_.peek();
		if (t.kind == TOK_RBRACE) { lex_.next(); return; }
		if (t.kind == TOK_EOF)
			throw ConfigException(locOf(t)
				+ "unclosed location block, expected '}'");
		parseLocationDirective(loc);
	}
}

void Parser::parseLocationDirective(LocationConfig& loc) {
	const Token& name = expect(TOK_WORD, "as directive name");

	if (name.text == "allowed_methods" || name.text == "methods") { parseAllowedMethods(loc); return; }
	if (name.text == "client_max_body_size") { parseLocClientMaxBodySize(loc); return; }
	if (name.text == "index")           { parseIndex(loc);          return; }
	if (name.text == "autoindex")       { parseAutoindex(loc);      return; }
	if (name.text == "return")          { parseReturn(loc);         return; }
	if (name.text == "root")            { parseLocRoot(loc);        return; }
	if (name.text == "upload_store")    { parseUploadStore(loc);    return; }
	if (name.text == "cgi" || name.text == "cgi_extension") { parseCgi(loc); return; }

	std::cerr << locOf(name)
			  << "warning: unknown location directive '" << name.text
			  << "', ignoring" << std::endl;
	skipToEndOfDirective();
}

void Parser::parseAllowedMethods(LocationConfig& loc) {
	while (lex_.peek().kind != TOK_SEMI) {
		const Token& t = lex_.peek();
		if (t.kind != TOK_WORD)
			throw ConfigException(locOf(t)
				+ "unexpected token in 'allowed_methods', got '" + t.text + "'");
		const std::string& m = t.text;
		if (m != "GET" && m != "POST" && m != "DELETE")
			throw ConfigException(locOf(t)
				+ "unsupported HTTP method '" + m + "' (allowed: GET, POST, DELETE)");
		loc.allowed_methods.push_back(m);
		lex_.next();
	}
	if (loc.allowed_methods.empty()) {
		throw ConfigException(locOf(lex_.peek())
			+ "'allowed_methods' needs at least one method");
	}
	expect(TOK_SEMI, "after 'allowed_methods' values");
}

// per-location `client_max_body_size 10M;` — overrides the server limit for
// requests routed to this location.
void Parser::parseLocClientMaxBodySize(LocationConfig& loc) {
	const Token& value = expect(TOK_WORD, "as 'client_max_body_size' argument");
	expect(TOK_SEMI, "after 'client_max_body_size' value");
	loc.client_max_body_size = parseSize(value);
	loc.has_client_max_body_size = true;
}

void Parser::parseIndex(LocationConfig& loc) {
	const Token& value = expect(TOK_WORD, "as 'index' argument");
	expect(TOK_SEMI, "after 'index' value");
	loc.index = value.text;
}

void Parser::parseLocRoot(LocationConfig& loc) {
	const Token& value = expect(TOK_WORD, "as 'root' argument");
	expect(TOK_SEMI, "after 'root' value");
	loc.root = value.text;
}

void Parser::parseUploadStore(LocationConfig& loc) {
	const Token& value = expect(TOK_WORD, "as 'upload_store' argument");
	expect(TOK_SEMI, "after 'upload_store' value");
	loc.upload_store = value.text;
}

void Parser::parseAutoindex(LocationConfig& loc) {
	const Token& value = expect(TOK_WORD, "as 'autoindex' argument (on|off)");
	expect(TOK_SEMI, "after 'autoindex' value");
	if (value.text == "on")       loc.autoindex = true;
	else if (value.text == "off") loc.autoindex = false;
	else throw ConfigException(locOf(value)
		+ "'autoindex' expects 'on' or 'off', got '" + value.text + "'");
}

void Parser::parseReturn(LocationConfig& loc) {
	const Token& codeTok   = expect(TOK_WORD, "as 'return' status code");
	const Token& targetTok = expect(TOK_WORD, "as 'return' target");
	expect(TOK_SEMI, "after 'return' target");

	loc.redirect.code    = parseStatusCode(codeTok);
	loc.redirect.target  = targetTok.text;
	loc.redirect.enabled = true;
}

void Parser::parseCgi(LocationConfig& loc) {
	const Token& ext = expect(TOK_WORD, "as 'cgi' extension (e.g. '.py')");

	if (ext.text.empty() || ext.text[0] != '.')
		throw ConfigException(locOf(ext)
			+ "'cgi' extension must start with '.': '" + ext.text + "'");

	// Interpreter is optional: "cgi .cgi;" means the script runs itself via its
	// shebang (the CGI engine handles an empty interpreter). "cgi .py /path;"
	// runs the script through the given interpreter.
	std::string interp;
	if (lex_.peek().kind == TOK_WORD)
		interp = lex_.next().text;
	expect(TOK_SEMI, "after 'cgi' directive");

	loc.cgi[ext.text] = interp;
}

void Parser::parseInclude(ServerConfig& server) {
	const Token& pathTok = expect(TOK_WORD, "as 'include' argument");
	expect(TOK_SEMI, "after 'include' value");

	const std::string resolved = joinPath(base_dir_, pathTok.text);

	if (cycle_guard_->count(resolved))
		throw ConfigException(locOf(pathTok)
			+ "include cycle: '" + resolved + "' is already being included");
	cycle_guard_->insert(resolved);

	std::string content = readIncludedFile(resolved);
	Lexer       subLex(content, resolved);
	Parser      sub(subLex, *cycle_guard_, dirnameOf(resolved));

	while (subLex.peek().kind != TOK_EOF)
		sub.parseServerDirective(server);

	cycle_guard_->erase(resolved);
}

// the value converters — turning a WORD's text into a real number.

long Parser::parseIntInRange(const Token& tok, long lo, long hi,
							 const char* what)
{
	if (tok.text.empty())
		throw ConfigException(locOf(tok)
			+ "expected " + what + ", got empty");

	for (std::size_t k = 0; k < tok.text.size(); ++k) {
		char c = tok.text[k];
		if (c < '0' || c > '9')
			throw ConfigException(locOf(tok)
				+ "expected " + what + ", got '" + tok.text + "'");
	}

	long n = 0;
	std::stringstream ss(tok.text);
	ss >> n;
	if (ss.fail() || !ss.eof())
		throw ConfigException(locOf(tok)
			+ "invalid " + what + ": '" + tok.text + "'");
	if (n < lo || n > hi) {
		std::stringstream msg;
		msg << locOf(tok) << what << " out of range ("
			<< lo << ".." << hi << "): " << tok.text;
		throw ConfigException(msg.str());
	}
	return n;
}

int Parser::parsePort(const Token& tok) {
	return static_cast<int>(parseIntInRange(tok, 1, 65535, "port"));
}

int Parser::parseStatusCode(const Token& tok) {
	return static_cast<int>(parseIntInRange(tok, 100, 599, "HTTP status code"));
}

std::size_t Parser::parseSize(const Token& tok) {
	const std::string& s = tok.text;
	if (s.empty())
		throw ConfigException(locOf(tok) + "empty size value");

	std::size_t       digits_end = s.size();
	std::size_t       multiplier = 1;

	char last = s[s.size() - 1];
	if (last == 'k' || last == 'K') {
		multiplier = 1024UL;
		--digits_end;
	} else if (last == 'm' || last == 'M') {
		multiplier = 1024UL * 1024UL;
		--digits_end;
	} else if (last == 'g' || last == 'G') {
		multiplier = 1024UL * 1024UL * 1024UL;
		--digits_end;
	}

	if (digits_end == 0)
		throw ConfigException(locOf(tok)
			+ "size has no number before suffix: '" + s + "'");

	Token digitTok(TOK_WORD, s.substr(0, digits_end), tok.line);
	long n = parseIntInRange(digitTok, 0, std::numeric_limits<long>::max(),
							 "size");

	std::size_t un = static_cast<std::size_t>(n);
	if (multiplier > 1
		&& un > std::numeric_limits<std::size_t>::max() / multiplier)
	{
		throw ConfigException(locOf(tok) + "size overflow: '" + s + "'");
	}
	return un * multiplier;
}

void Parser::splitHostPort(const Token& tok,
						   std::string& host_out, int& port_out)
{
	const std::string& s = tok.text;

	// search for the LAST ':' so a bracketed IPv6 host like "[::1]:8080" still
	// splits correctly — even though I don't actually serve IPv6 here.
	std::string::size_type colon = s.rfind(':');
	if (colon == std::string::npos) {
		host_out = "0.0.0.0";
		Token portTok(TOK_WORD, s, tok.line);
		port_out = parsePort(portTok);
		return;
	}

	std::string host = s.substr(0, colon);
	std::string port = s.substr(colon + 1);

	if (host.size() >= 2 && host[0] == '[' && host[host.size() - 1] == ']')
		host = host.substr(1, host.size() - 2);

	if (host.empty())
		throw ConfigException(locOf(tok)
			+ "empty host in 'listen' value '" + s + "'");

	host_out = host;
	Token portTok(TOK_WORD, port, tok.line);
	port_out = parsePort(portTok);
}

} // namespace extcfg
