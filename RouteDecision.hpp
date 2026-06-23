/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   RouteDecision.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/21 16:12:29 by gansari           #+#    #+#             */
/*   Updated: 2026/06/23 12:54:22 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef ROUTEDECISION_HPP
# define ROUTEDECISION_HPP

# include <string>
# include "LocationConfig.hpp"

struct RouteDecision
{
	enum Kind
	{
		KIND_SERVE, // serve a filesystem path
		KIND_REDIRECT, // emit a 3xx with Location header
		KIND_ERROR, // emit an error status (e.g. 405, 404, 413)
		KIND_CGI // run a CGI script (handled by Client + CgiSession)
	};

	Kind		kind;

	// --- KIND_SERVE fields ---
	// The resolved filesystem path: location root + URI tail.
	// e.g. URI "/foo/bar.html" with root "/var/www" -> "/var/www/foo/bar.html"
	std::string	fs_path;

	// True if the resolved path ends in '/' (directory request)
	bool		is_directory_request;

	// Default index file from the matched location (may be empty)
	std::string	index_file;

	// Directory-listing enabled flag from the matched location
	bool		autoindex;

	// --- KIND_REDIRECT fields ---
	int			redirect_code;   // 301, 302, etc
	std::string	redirect_url;    // value for Location header

	// --- KIND_CGI fields ---
	std::string	cgi_interpreter;  // e.g. "/usr/bin/python3"

	// --- KIND_ERROR fields ---
	int			error_code;      // 404, 405, 413, ...

	// --- Shared metadata, useful for any kind ---
	long		effective_body_limit;

	// Pointer (non-owning) to the matched location
	const LocationConfig*	location;

	RouteDecision();
};

#endif
