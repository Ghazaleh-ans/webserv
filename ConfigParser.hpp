/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigParser.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/12 15:59:32 by gansari           #+#    #+#             */
/*   Updated: 2026/05/13 13:02:08 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CONFIGPARSER_HPP
# define CONFIGPARSER_HPP

# include <string>
# include <vector>
# include <stdexcept>
# include "Tokenizer.hpp"
# include "ServerConfig.hpp"
# include "LocationConfig.hpp"

class ConfigParser
{
public:
	ConfigParser();
	~ConfigParser();

	std::vector<ServerConfig>	parse_file(const std::string& path);
	std::vector<ServerConfig>	parse_string(const std::string& input);

private:
	std::vector<Token>	_tokens;
	size_t				_pos;

	// --- Token cursor helpers ---
	const Token&	peek() const;
	const Token&	consume();
	bool			match(Token::Type t) const;
	void			expect(Token::Type t, const std::string& what);

	// --- Grammar rules ---
	ServerConfig	parse_server();
	LocationConfig	parse_location();
	void			parse_server_directive(ServerConfig& srv);
	void			parse_location_directive(LocationConfig& loc);

	// --- Value parsers (each consumes its tokens and the trailing ';') ---
	void			parse_listen(ServerConfig& srv);
	void			parse_host(ServerConfig& srv);
	void			parse_server_name(ServerConfig& srv);
	void			parse_body_size(long& target);
	void			parse_error_page(ServerConfig& srv);
	void			parse_methods(LocationConfig& loc);
	void			parse_return(LocationConfig& loc);
	void			parse_autoindex(LocationConfig& loc);
	void			parse_cgi(LocationConfig& loc);

	// Consumes one WORD token, errors if next isn't a word.
	std::string		expect_word(const std::string& what);
	// Consumes trailing ';' with a clear error message.
	void			expect_semi(const std::string& directive);

	// --- Semantic validation, run after parsing ---
	void			validate(const std::vector<ServerConfig>& servers);
	void			validate_server(const ServerConfig& srv);
	void			validate_location(const LocationConfig& loc,
									   const std::string& server_ctx);

	// --- Utility ---
	// Throws a runtime_error formatted with the token's line number.
	void	error(const std::string& msg, int line) const;
	// Convert "1048576", "1m", "10K" etc. to bytes. Returns false on bad format.
	bool	parse_size(const std::string& s, long& out) const;
	// Convert "12345" to int with range check. Returns false on bad format.
	bool	parse_int(const std::string& s, int& out) const;
};

#endif
