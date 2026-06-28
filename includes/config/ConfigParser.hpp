/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigParser.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/12 15:59:32 by gansari           #+#    #+#             */
/*   Updated: 2026/06/24 11:02:40 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CONFIGPARSER_HPP
# define CONFIGPARSER_HPP

# include <string>
# include <vector>
# include <stdexcept>
# include "config/Tokenizer.hpp"
# include "config/ServerConfig.hpp"
# include "config/LocationConfig.hpp"

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

	const Token&	peek() const;
	const Token&	consume();
	bool			match(Token::Type t) const;
	void			expect(Token::Type t, const std::string& what);

	ServerConfig	parse_server();
	LocationConfig	parse_location();
	void			parse_server_directive(ServerConfig& srv);
	void			parse_location_directive(LocationConfig& loc);

	void			parse_listen(ServerConfig& srv);
	void			parse_host(ServerConfig& srv);
	void			parse_server_name(ServerConfig& srv);
	void			parse_body_size(long& target);
	void			parse_error_page(ServerConfig& srv);
	void			parse_methods(LocationConfig& loc);
	void			parse_return(LocationConfig& loc);
	void			parse_autoindex(LocationConfig& loc);
	void			parse_cgi(LocationConfig& loc);

	std::string		expect_word(const std::string& what);
	void			expect_semi(const std::string& directive);

	void			validate(const std::vector<ServerConfig>& servers);
	void			validate_server(const ServerConfig& srv);
	void			validate_location(const LocationConfig& loc,
									   const std::string& server_ctx);

	void	error(const std::string& msg, int line) const;
	bool	parse_size(const std::string& s, long& out) const;
	bool	parse_int(const std::string& s, int& out) const;
};

#endif
