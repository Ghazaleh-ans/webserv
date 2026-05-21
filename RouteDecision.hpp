/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   RouteDecision.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/21 16:12:29 by gansari           #+#    #+#             */
/*   Updated: 2026/05/21 16:54:26 by gansari          ###   ########.fr       */
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
		KIND_SERVE,      // serve a filesystem path (Module 5 reads the file)
		KIND_REDIRECT,   // emit a 3xx with Location header
		KIND_ERROR,      // emit an error status (e.g. 405, 404, 413)
		KIND_CGI         // run a CGI script (Module 8) — not used yet
	};

	Kind		kind;

	// --- KIND_SERVE fields ---
	// The resolved filesystem path: location root + URI tail.
	// e.g. URI "/foo/bar.html" with root "/var/www" → "/var/www/foo/bar.html"
	std::string	fs_path;

	// True if the resolved path ends in '/' (directory request).
	// Module 5 uses this to decide between index file lookup and autoindex.
	bool		is_directory_request;

	// Default index file from the matched location (may be empty).
	std::string	index_file;

	// Directory-listing enabled flag from the matched location.
	bool		autoindex;

	// Allowed methods on this route (already validated to include
	// the request's method by the time we get here).
	// Passed through so Module 5 can populate the Allow header on a
	// 405 if it needs to.

	// --- KIND_REDIRECT fields ---
	int			redirect_code;   // 301, 302, etc.
	std::string	redirect_url;    // value for Location header

	// --- KIND_ERROR fields ---
	int			error_code;      // 404, 405, 413, ...

	// --- Shared metadata, useful for any kind ---
	// Effective body-size cap for this request: location override if set,
	// otherwise server default. Stored here so the caller doesn't need
	// to re-derive it. (Currently informational; Module 7 will enforce
	// per-route upload limits using it.)
	long		effective_body_limit;

	// Pointer (non-owning) to the matched location, if any. Module 5
	// uses this to find custom upload_store, cgi_handlers, etc.
	// NULL if no location matched (in which case kind == KIND_ERROR
	// with error_code == 404).
	const LocationConfig*	location;

	RouteDecision();
};

#endif
