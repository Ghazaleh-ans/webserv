/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerConfig.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/12 13:33:31 by gansari           #+#    #+#             */
/*   Updated: 2026/05/12 13:33:32 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVERCONFIG_HPP
# define SERVERCONFIG_HPP

# include <string>
# include <vector>
# include <map>
# include "LocationConfig.hpp"

// Represents one `server { ... }` block.
// A config file can contain multiple of these (each defining a virtual host
// or a different listen socket).
class ServerConfig
{
public:
	ServerConfig();
	~ServerConfig();

	// IP to bind to. "0.0.0.0" means all interfaces. Default in ctor.
	std::string					host;

	// TCP port. 0 is invalid; we use it as "not set" sentinel and
	// the validator will reject it.
	int							port;

	// server_name directive. For pure listen:port routing this is unused,
	// but we parse it anyway so configs that include it don't break.
	std::vector<std::string>	server_names;

	// Body size limit in bytes. Default 1 MiB.
	long						client_max_body_size;

	// Custom error pages: status code -> path relative to a location's root.
	// e.g. error_pages[404] = "/errors/404.html"
	std::map<int, std::string>	error_pages;

	// All `location` blocks inside this server.
	// Order matters: we use longest-prefix matching at request time,
	// but ties are broken by config order, so we keep a vector (not a map).
	std::vector<LocationConfig>	locations;
};

#endif
