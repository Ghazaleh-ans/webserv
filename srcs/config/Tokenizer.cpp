/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Tokenizer.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mibrokhimov <contact@ibrokhimov.de>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/12 15:58:01 by gansari           #+#    #+#             */
/*   Updated: 2026/07/04 18:51:11 by mibrokhimov      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "config/Tokenizer.hpp"
#include <stdexcept>
#include <sstream>
#include <cctype>

Tokenizer::Tokenizer() : _pos(0), _line(1) {}
Tokenizer::~Tokenizer() {}

void	Tokenizer::skip_whitespace_and_comments(const std::string& input)
{
	while (_pos < input.size())
	{
		char c = input[_pos];
		if (c == '\n')
		{
			++_line;
			++_pos;
		}
		else if (std::isspace(static_cast<unsigned char>(c)))
		{
			++_pos;
		}
		else if (c == '#')
		{
			while (_pos < input.size() && input[_pos] != '\n')
				++_pos;
		}
		else
		{
			break;
		}
	}
}

Token	Tokenizer::read_word(const std::string& input)
{
	Token tok;
	tok.type = Token::WORD;
	tok.line = _line;

	size_t start = _pos;
	while (_pos < input.size())
	{
		char c = input[_pos];
		if (std::isspace(static_cast<unsigned char>(c)))
			break;
		if (c == '{' || c == '}' || c == ';' || c == '#')
			break;
		++_pos;
	}
	tok.value = input.substr(start, _pos - start);
	return tok;
}

std::vector<Token>	Tokenizer::tokenize(const std::string& input)
{
	std::vector<Token> tokens;
	_pos = 0;
	_line = 1;

	while (_pos < input.size())
	{
		skip_whitespace_and_comments(input);
		if (_pos >= input.size())
			break;

		char c = input[_pos];
		Token tok;
		tok.line = _line;

		if (c == '{')
		{
			tok.type = Token::LBRACE;
			++_pos;
			tokens.push_back(tok);
		}
		else if (c == '}')
		{
			tok.type = Token::RBRACE;
			++_pos;
			tokens.push_back(tok);
		}
		else if (c == ';')
		{
			tok.type = Token::SEMI;
			++_pos;
			tokens.push_back(tok);
		}
		else
		{
			tokens.push_back(read_word(input));
		}
	}

	Token end_tok;
	end_tok.type = Token::END;
	end_tok.line = _line;
	tokens.push_back(end_tok);

	return tokens;
}
