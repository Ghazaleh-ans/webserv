/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Router.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/21 15:34:53 by gansari           #+#    #+#             */
/*   Updated: 2026/06/23 12:42:26 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef ROUTER_HPP
# define ROUTER_HPP

# include "ServerConfig.hpp"
# include "HttpRequest.hpp"
# include "RouteDecision.hpp"

class Router
{
public:
	Router();
	~Router();


	RouteDecision	route(const HttpRequest& req, const ServerConfig& server) const;

private:
	// Find the location whose `path` is the longest prefix of `uri_path`
	const LocationConfig*	match_location(const std::string& uri_path, const ServerConfig& server) const;

	bool	method_allowed(const LocationConfig& loc, const std::string& method) const;

	// Build the filesystem path: location root + URI portion after the
	// location's matched prefix.
	// Example: location "/files" with root "/var/www", URI "/files/foo.txt"
	//   -> strip "/files" from "/files/foo.txt" -> "/foo.txt"
	//   -> "/var/www" + "/foo.txt" -> "/var/www/foo.txt"
	std::string	build_fs_path(const std::string& uri_path, const LocationConfig& loc) const;

	// Pick the effective body limit: location override if set
	long	effective_body_limit(const LocationConfig& loc, const ServerConfig& server) const;
};

#endif
