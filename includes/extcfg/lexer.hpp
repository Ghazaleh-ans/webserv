#pragma once

#include <string>
#include <vector>
#include <cstddef>   // std::size_t

namespace extcfg
{

// the whole token vocabulary for my config language — just five kinds, that's
// genuinely all the nginx-style grammar I accept needs.
enum TokenKind {
	TOK_WORD,    // any non-whitespace, non-special run of characters
	TOK_LBRACE,  // "{"
	TOK_RBRACE,  // "}"
	TOK_SEMI,    // ";"
	TOK_EOF      // sticky: lexer keeps returning this once the stream ends
};

// one token out of the lexer.
struct Token {
	TokenKind   kind;
	std::string text;
	int         line;

	Token();
	Token(TokenKind k, const std::string& t, int l);
};

// Lexer: source string in, token stream out. Eager — the ctor tokenises the
// whole input at once into a vector, and the parser walks it with peek()/next().
class Lexer {
	public:
		Lexer(const std::string& source, const std::string& origin);

		const Token& peek() const;
		const Token& next();
		const std::string& origin() const;

	private:
		std::vector<Token> tokens_;
		std::string        origin_;
		std::size_t        pos_;

		void tokenize(const std::string& source);

		Lexer(const Lexer&);
		Lexer& operator=(const Lexer&);
};

} // namespace extcfg
