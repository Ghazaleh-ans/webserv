/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Router.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/21 15:34:53 by gansari           #+#    #+#             */
/*   Updated: 2026/06/01 18:02:58 by gansari          ###   ########.fr       */
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

	// The single public entry point. Never throws.
	// Always returns a valid RouteDecision (worst case: KIND_ERROR/500).
	RouteDecision	route(const HttpRequest& req,
						   const ServerConfig& server) const;

private:
	// Find the location whose `path` is the longest prefix of `uri_path`.
	// Returns NULL if no location matches (caller will produce 404).
	const LocationConfig*	match_location(const std::string& uri_path,
										   const ServerConfig& server) const;

	// True if `loc.methods` contains `method`. An empty methods list
	// means "default: GET only" per the parser's convention.
	bool	method_allowed(const LocationConfig& loc,
						   const std::string& method) const;

	// Build the filesystem path: location root + URI portion after the
	// location's matched prefix.
	// Example: location "/files" with root "/var/www", URI "/files/foo.txt"
	//   → strip "/files" from "/files/foo.txt" → "/foo.txt"
	//   → "/var/www" + "/foo.txt" → "/var/www/foo.txt"
	//
	// Does NOT do any path-traversal protection here — that's resolve_path's
	// job, called by Module 5 after this when it actually opens the file.
	std::string	build_fs_path(const std::string& uri_path,
							  const LocationConfig& loc) const;

	// Pick the effective body limit: location override if set
	// (client_max_body_size >= 0), otherwise the server default.
	long	effective_body_limit(const LocationConfig& loc,
								 const ServerConfig& server) const;
};

#endif
