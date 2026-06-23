/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Router.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/21 16:12:44 by gansari           #+#    #+#             */
/*   Updated: 2026/06/23 13:40:10 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Router.hpp"
#include <sys/stat.h>

Router::Router() {}
Router::~Router() {}

// For URI "/cgi-bin/script.py" against locations ["/", "/cgi-bin"]:
//   "/"        matches with length 1
//   "/cgi-bin" matches with length 8  -> longest, wins
//
// For URI "/anything" against just "/", "/" wins (always matches everything)
//
// The tricky case: location "/foo" should NOT match URI "/foobar"
// the prefix must be followed by '/' or end-of-uri. Otherwise "/foo" would
// claim "/foobar" and the user would get the wrong file
const LocationConfig*	Router::match_location(const std::string& uri_path, const ServerConfig& server) const
{
	const LocationConfig*	best = NULL;
	size_t					best_len = 0;

	for (size_t i = 0; i < server.locations.size(); ++i)
	{
		const LocationConfig& loc = server.locations[i];
		const std::string& p = loc.path;

		// If location's path isn't a prefix of uri_path -> go to the next one
		if (uri_path.compare(0, p.size(), p) != 0)
			continue;

		if (p.size() < uri_path.size())
		{
			// "/foo" must be followed by '/' in "/foo/bar"
			// but "/foobar" must NOT match "/foo" -> check the char right after the prefix
			// Exception: if p ends with '/', the boundary is already inside p
			if (!p.empty() && p[p.size() - 1] != '/' && uri_path[p.size()] != '/')
				continue;
		}

		if (p.size() > best_len)
		{
			best = &loc;
			best_len = p.size();
		}
	}

	return best;
}

bool	Router::method_allowed(const LocationConfig& loc, const std::string& method) const
{
	if (loc.methods.empty())
		return method == "GET";

	for (size_t i = 0; i < loc.methods.size(); ++i)
	{
		if (loc.methods[i] == method)
			return true;
	}
	return false;
}

// convert a URL path into an actual file path on hard drive
// So we strip the location's prefix from the URI, then prepend root
std::string	Router::build_fs_path(const std::string& uri_path, const LocationConfig& loc) const
{
	// Strip the location's prefix. After this, `tail` either starts
	// with '/' or is empty (when uri exactly equals the location)
	std::string tail;
	if (uri_path.size() > loc.path.size())
		tail = uri_path.substr(loc.path.size());

	std::string root = loc.root;

	if (tail.empty())
		return root;

	// Ensure exactly one '/' between root and tail
	bool root_slash = !root.empty() && root[root.size() - 1] == '/';
	bool tail_slash = tail[0] == '/';

	if (root_slash && tail_slash)
		root.resize(root.size() - 1);
	else if (!root_slash && !tail_slash)
		return root + "/" + tail;

	return root + tail;
}

// LocationConfig defaults client_max_body_size -> -1 ("not set, inherit")
long	Router::effective_body_limit(const LocationConfig& loc, const ServerConfig& server) const
{
	if (loc.client_max_body_size >= 0)
		return loc.client_max_body_size;
	return server.client_max_body_size;
}

// ============================================================
// The main entry point
// ============================================================
// Order of checks matters:
//   1. Find a matching location.       (no match -> 404)
//   2. If location has `return` -> emit redirect immediately,
//      skipping the method check. NGINX does this too; the rationale
//      is that a redirect doesn't actually consume the resource, so
//      methods don't apply.
//   3. Check method is allowed.        (not allowed -> 405)
//   4. Compute fs_path and metadata later
RouteDecision	Router::route(const HttpRequest& req, const ServerConfig& server) const
{
	RouteDecision d;

	const LocationConfig* loc = match_location(req.path, server);
	if (loc == NULL)
	{
		d.kind = RouteDecision::KIND_ERROR;
		d.error_code = 404;
		d.effective_body_limit = server.client_max_body_size;
		return d;
	}

	d.location = loc;
	d.effective_body_limit = effective_body_limit(*loc, server);

	// Redirect short-circuits everything
	if (loc->has_redirect)
	{
		d.kind = RouteDecision::KIND_REDIRECT;
		d.redirect_code = loc->redirect_code;
		d.redirect_url = loc->redirect_url;
		return d;
	}

	if (!method_allowed(*loc, req.method))
	{
		d.kind = RouteDecision::KIND_ERROR;
		d.error_code = 405;
		return d;
	}

	// At this point we know we want to serve something
	d.kind = RouteDecision::KIND_SERVE;
	d.fs_path = build_fs_path(req.path, *loc);
	// /images/cat.jpg -> file request
	// /images/ -> directory request(trailing slash)
	d.is_directory_request = !req.path.empty() && req.path[req.path.size() - 1] == '/';
	d.index_file = loc->index;
	d.autoindex = loc->autoindex;

	// CGI detection: 
	// If the resolved file's extension matches one of the location's cgi_handlers -> switch to KIND_CGI
	if (!loc->cgi_handlers.empty() && !d.is_directory_request)
	{
		size_t dot = d.fs_path.find_last_of('.');
		size_t slash = d.fs_path.find_last_of('/');
		if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) // make sure doc is not in the directory name
		{
			std::string ext = d.fs_path.substr(dot);  //".py", ".php", ...
			std::map<std::string, std::string>::const_iterator it = loc->cgi_handlers.find(ext);
			if (it != loc->cgi_handlers.end())
			{
				struct stat st;
				if (stat(d.fs_path.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) // check if it exists and is a regular file
				{
					d.kind = RouteDecision::KIND_ERROR;
					d.error_code = 404;
					return d;
				}
				d.kind = RouteDecision::KIND_CGI;
				d.cgi_interpreter = it->second;
			}
		}
	}
	return d;
}
