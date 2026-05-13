/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerConfig.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/12 13:33:31 by gansari           #+#    #+#             */
/*   Updated: 2026/05/13 12:42:56 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVERCONFIG_HPP
# define SERVERCONFIG_HPP

# include <string>
# include <vector>
# include <map>
# include "LocationConfig.hpp"

class ServerConfig
{
public:
	ServerConfig();
	~ServerConfig();

	std::string					host;
	int							port;

	std::vector<std::string>	server_names;
	long						client_max_body_size;
	std::map<int, std::string>	error_pages;
	std::vector<LocationConfig>	locations;
};

#endif
