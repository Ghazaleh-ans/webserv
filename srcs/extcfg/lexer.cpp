#include "extcfg/lexer.hpp"

// the design notes are in includes/extcfg/lexer.hpp. this file is the boring
// part: one loop over the source string spitting tokens into a vector.

namespace extcfg
{

Token::Token()
	: kind(TOK_EOF), text(""), line(0)
{}

Token::Token(TokenKind k, const std::string& t, int l)
	: kind(k), text(t), line(l)
{}

Lexer::Lexer(const std::string& source, const std::string& origin)
	: tokens_(), origin_(origin), pos_(0)
{
	tokenize(source);
}

const Token& Lexer::peek() const {
	return tokens_[pos_];
}

const Token& Lexer::next() {
	if (tokens_[pos_].kind == TOK_EOF)
		return tokens_[pos_];
	return tokens_[pos_++];
}

const std::string& Lexer::origin() const {
	return origin_;
}

static bool isWhitespace(char c) {
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static bool isSpecial(char c) {
	return c == '{' || c == '}' || c == ';' || c == '#';
}

void Lexer::tokenize(const std::string& src) {
	int         line = 1;
	std::size_t i    = 0;

	while (i < src.size()) {
		char c = src[i];

		if (c == '\n') {
			++line;
			++i;
			continue;
		}

		if (isWhitespace(c)) {
			++i;
			continue;
		}

		if (c == '#') {
			while (i < src.size() && src[i] != '\n')
				++i;
			continue;
		}

		if (c == '{') { tokens_.push_back(Token(TOK_LBRACE, "{", line)); ++i; continue; }
		if (c == '}') { tokens_.push_back(Token(TOK_RBRACE, "}", line)); ++i; continue; }
		if (c == ';') { tokens_.push_back(Token(TOK_SEMI,   ";", line)); ++i; continue; }

		std::string word;
		while (i < src.size()) {
			char ch = src[i];
			if (isWhitespace(ch) || isSpecial(ch))
				break;
			word += ch;
			++i;
		}
		tokens_.push_back(Token(TOK_WORD, word, line));
	}

	tokens_.push_back(Token(TOK_EOF, "", line));
}

} // namespace extcfg
