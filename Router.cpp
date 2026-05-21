/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Router.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/21 16:12:44 by gansari           #+#    #+#             */
/*   Updated: 2026/05/21 18:29:13 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Router.hpp"

Router::Router() {}
Router::~Router() {}

// ============================================================
// match_location: longest-prefix matching with config-order tiebreak
// ============================================================
// For URI "/cgi-bin/script.py" against locations ["/", "/cgi-bin"]:
//   "/"        matches with length 1
//   "/cgi-bin" matches with length 8  ← longest, wins
//
// For URI "/anything" against just "/", "/" wins (always matches everything).
//
// The tricky case: location "/foo" should NOT match URI "/foobar" — the
// prefix must be followed by '/' or end-of-uri. Otherwise "/foo" would
// claim "/foobar" and the user would get the wrong file.
//
// We walk the locations vector once. Iteration order = config order,
// so when two locations share a length, the first one wins — that's
// the documented NGINX behavior.
const LocationConfig*	Router::match_location(const std::string& uri_path,
											   const ServerConfig& server) const
{
	const LocationConfig*	best = NULL;
	size_t					best_len = 0;

	for (size_t i = 0; i < server.locations.size(); ++i)
	{
		const LocationConfig& loc = server.locations[i];
		const std::string& p = loc.path;

		// Reject if location's path isn't a prefix of uri_path.
		if (uri_path.compare(0, p.size(), p) != 0)
			continue;

		// Boundary check: prefix must end at '/' or end-of-string in uri_path,
		// UNLESS the prefix itself ends in '/' (then we already matched on
		// the slash boundary). The special case is location "/" which
		// matches anything.
		if (p.size() < uri_path.size())
		{
			// "/foo" must be followed by '/' in "/foo/bar" — but "/foobar"
			// must NOT match "/foo". So check the char right after the prefix.
			// Exception: if p ends with '/', the boundary is already inside p.
			if (!p.empty() && p[p.size() - 1] != '/'
				&& uri_path[p.size()] != '/')
				continue;
		}

		// Take only if strictly longer than current best, so equal-length
		// hits (which shouldn't happen since the parser rejects duplicates,
		// but defensively) keep the first one.
		if (p.size() > best_len)
		{
			best = &loc;
			best_len = p.size();
		}
	}

	return best;
}

// ============================================================
// method_allowed: empty list defaults to "GET only"
// ============================================================
// The config parser leaves `methods` empty when the user didn't write
// a `methods` directive. The convention is: missing == "GET only".
// This is the safest default — GET is harmless; uploads/deletes
// require explicit opt-in.
bool	Router::method_allowed(const LocationConfig& loc,
							   const std::string& method) const
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

// ============================================================
// build_fs_path: subject example translated to code
// ============================================================
// Subject (Chapter IV.3):
//   "if URL /kapouet is rooted to /tmp/www, URL /kapouet/pouic/toto/pouet
//    will search for /tmp/www/pouic/toto/pouet"
//
// So we strip the location's prefix from the URI, then prepend root.
// Special care:
//   - Leave the leading '/' in the tail so root + tail is well-formed
//   - Don't double-slash (root ending in '/' + tail starting with '/')
std::string	Router::build_fs_path(const std::string& uri_path,
								  const LocationConfig& loc) const
{
	// Strip the location's prefix. After this, `tail` either starts
	// with '/' or is empty (when uri exactly equals the location).
	std::string tail;
	if (uri_path.size() > loc.path.size())
		tail = uri_path.substr(loc.path.size());

	std::string root = loc.root;

	// Avoid double slash:
	if (!root.empty() && root[root.size() - 1] == '/'
		&& !tail.empty() && tail[0] == '/')
		root.resize(root.size() - 1);

	// If tail is empty AND uri exactly equals the location path,
	// the user is requesting the location's root directory itself.
	// Make sure we produce "/var/www" (no trailing junk) so Module 5
	// knows to treat it as a directory request.
	if (tail.empty())
		return root;

	return root + tail;
}

// ============================================================
// effective_body_limit: location overrides server default
// ============================================================
// LocationConfig defaults client_max_body_size to -1 ("not set, inherit").
// Any non-negative value is an explicit override.
long	Router::effective_body_limit(const LocationConfig& loc,
									 const ServerConfig& server) const
{
	if (loc.client_max_body_size >= 0)
		return loc.client_max_body_size;
	return server.client_max_body_size;
}

// ============================================================
// The main entry point
// ============================================================
// Order of checks matters:
//   1. Find a matching location.       (no match → 404)
//   2. If location has `return` → emit redirect immediately,
//      skipping the method check. NGINX does this too; the rationale
//      is that a redirect doesn't actually consume the resource, so
//      methods don't apply.
//   3. Check method is allowed.        (not allowed → 405)
//   4. Compute fs_path and metadata later
RouteDecision	Router::route(const HttpRequest& req,
							   const ServerConfig& server) const
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

	// Redirect short-circuits everything.
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

	// At this point we know we want to serve something.
	d.kind = RouteDecision::KIND_SERVE;
	d.fs_path = build_fs_path(req.path, *loc);
	d.is_directory_request = !req.path.empty()
		&& req.path[req.path.size() - 1] == '/';
	d.index_file = loc->index;
	d.autoindex = loc->autoindex;

	return d;
}
