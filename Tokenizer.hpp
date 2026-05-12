/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Tokenizer.hpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/12 13:33:06 by gansari           #+#    #+#             */
/*   Updated: 2026/05/12 13:33:07 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef TOKENIZER_HPP
# define TOKENIZER_HPP

# include <string>
# include <vector>

// A single token from the config file.
// We keep the line number so error messages can say
// "unexpected '}' on line 17" instead of just "syntax error".
struct Token
{
	enum Type
	{
		WORD,    // anything that isn't a special char: "listen", "8080", "/var/www"
		LBRACE,  // {
		RBRACE,  // }
		SEMI,    // ;
		END      // end-of-input sentinel
	};

	Type		type;
	std::string	value;   // empty for LBRACE/RBRACE/SEMI/END
	int			line;
};

// Stateless tokenizer: feed it a string, get back a vector of tokens.
// A free function would also work, but a class lets us cleanly group
// the helpers (skip_whitespace, read_word) without polluting the
// global namespace.
class Tokenizer
{
public:
	Tokenizer();
	~Tokenizer();

	// Returns tokens in order, terminated by an END token.
	// Throws std::runtime_error on lexical errors (e.g. unterminated string).
	std::vector<Token>	tokenize(const std::string& input);

private:
	// Position tracking — these are reset at the start of each tokenize() call.
	size_t	_pos;
	int		_line;

	void	skip_whitespace_and_comments(const std::string& input);
	Token	read_word(const std::string& input);
};

#endif
