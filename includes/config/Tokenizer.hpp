/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Tokenizer.hpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mibrokhimov <contact@ibrokhimov.de>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/12 13:33:06 by gansari           #+#    #+#             */
/*   Updated: 2026/07/04 18:51:11 by mibrokhimov      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef TOKENIZER_HPP
# define TOKENIZER_HPP

# include <string>
# include <vector>

struct Token
{
	enum Type
	{
		WORD, // anything that isn't a special char: "listen", "8080", "/var/www"
		LBRACE, // {
		RBRACE, // }
		SEMI, // ;
		END // end-of-input sentinel
	};

	Type		type;
	std::string	value; // empty for LBRACE/RBRACE/SEMI/END -> for WORD
	int			line;
};

class Tokenizer
{
public:
	Tokenizer();
	~Tokenizer();

	std::vector<Token>	tokenize(const std::string& input);

private:
	size_t	_pos;
	int		_line;

	void	skip_whitespace_and_comments(const std::string& input);
	Token	read_word(const std::string& input);
};

#endif
