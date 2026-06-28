/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   LocationConfig.hpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/12 13:07:42 by gansari           #+#    #+#             */
/*   Updated: 2026/06/23 13:32:02 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef LOCATIONCONFIG_HPP
# define LOCATIONCONFIG_HPP

# include <string>
# include <vector>
# include <map>

class LocationConfig
{
public:
	LocationConfig();
	~LocationConfig();

	std::string					path;
	std::string					root;
	std::string					index;
	std::vector<std::string>	methods;
	bool						has_redirect;
	int							redirect_code;
	std::string					redirect_url;
	bool						autoindex;
	long						client_max_body_size;
	std::string					upload_store;

	// CGI configuration: extension (".py") -> interpreter ("/usr/bin/python3")
	// Using a map because each extension maps to exactly one interpreter,
	std::map<std::string, std::string>	cgi_handlers;
};

#endif
