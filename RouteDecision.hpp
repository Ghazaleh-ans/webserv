/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   RouteDecision.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/21 16:12:29 by gansari           #+#    #+#             */
/*   Updated: 2026/06/24 15:42:18 by gansari          ###   ########.fr       */
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
	bool		is_directory_request;
	std::string	index_file;
	bool		autoindex;
	int			redirect_code;   // 301, 302, etc
	std::string	redirect_url;    // value for Location header
	std::string	cgi_interpreter;  // e.g. "/usr/bin/python3"
	int			error_code;      // 404, 405, 413, ...
	long		effective_body_limit;
	const LocationConfig*	location;

	RouteDecision();
};

#endif
