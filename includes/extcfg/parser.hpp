#pragma once

#include <string>
#include <vector>
#include <set>
#include "extcfg/config.hpp"
#include "extcfg/lexer.hpp"

namespace extcfg
{

// Parser: eats the Lexer's token stream and builds a list of ServerConfig
// structs. Recursive-descent — one method per grammar production. Anything
// malformed throws ConfigException reading "<origin>:<line>: <what broke>".
class Parser {
	public:
		Parser(Lexer& lex);

		void parseFile(std::vector<ServerConfig>& out);

	private:
		Parser(Lexer& lex, std::set<std::string>& shared_cycle_guard,
			   const std::string& base_dir);

		Lexer& lex_;

		std::set<std::string>  owned_cycle_guard_;
		std::set<std::string>* cycle_guard_;
		std::string base_dir_;

		bool match(TokenKind kind);
		const Token& expect(TokenKind kind, const char* context);
		std::string locOf(const Token& t) const;

		void parseServerBlock(ServerConfig& server);
		void parseServerDirective(ServerConfig& server);
		void parseListen(ServerConfig& server);
		void parseHost(ServerConfig& server);
		void parseServerName(ServerConfig& server);
		void parseRoot(ServerConfig& server);
		void parseClientMaxBodySize(ServerConfig& server);
		void parseErrorPage(ServerConfig& server);

		void parseLocation(ServerConfig& server);
		void parseLocationBlock(LocationConfig& loc);
		void parseLocationDirective(LocationConfig& loc);

		void parseAllowedMethods(LocationConfig& loc);
		void parseLocClientMaxBodySize(LocationConfig& loc);
		void parseIndex(LocationConfig& loc);
		void parseAutoindex(LocationConfig& loc);
		void parseReturn(LocationConfig& loc);
		void parseLocRoot(LocationConfig& loc);     // location-level override
		void parseUploadStore(LocationConfig& loc);
		void parseCgi(LocationConfig& loc);

		void parseInclude(ServerConfig& server);

		void skipToEndOfDirective();

		long parseIntInRange(const Token& tok, long lo, long hi,
							 const char* what);
		int parsePort(const Token& tok);
		int parseStatusCode(const Token& tok);
		std::size_t parseSize(const Token& tok);
		void splitHostPort(const Token& tok,
						   std::string& host_out, int& port_out);

		Parser(const Parser&);
		Parser& operator=(const Parser&);
};

} // namespace extcfg
