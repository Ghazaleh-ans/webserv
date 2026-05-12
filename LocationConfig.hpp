/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   LocationConfig.hpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/12 13:07:42 by gansari           #+#    #+#             */
/*   Updated: 2026/05/12 13:18:12 by gansari          ###   ########.fr       */
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

	// The URI prefix this block matches, e.g. "/", "/upload", "/cgi-bin"
	std::string					path;

	// Filesystem root. URL "/foo/bar.html" with root "/var/www"
	// resolves to "/var/www/foo/bar.html".
	std::string					root;

	// Default file when the request targets a directory (e.g. "index.html")
	std::string					index;

	// Allowed HTTP methods on this route. Empty == "inherit default" (GET only).
	std::vector<std::string>	methods;

	// If non-empty, every request to this location is redirected.
	// First = status code (301/302/...), second = target URL.
	bool						has_redirect;
	int							redirect_code;
	std::string					redirect_url;

	// Directory listing when no index file is found
	bool						autoindex;

	// Per-location override of the server-wide client_max_body_size.
	// -1 means "not set, inherit from server".
	long						client_max_body_size;

	// Where uploads land. Empty == uploads disabled on this route.
	std::string					upload_store;

	// CGI configuration: extension (".py") -> interpreter ("/usr/bin/python3")
	// Using a map because each extension maps to exactly one interpreter,
	// and lookup by extension is the hot path during request handling.
	/*
	CGI (Common Gateway Interface) is a standard that lets a web server execute external programs and return their output as an HTTP response — the classic way to run dynamic scripts (Python, PHP, Perl, etc.) through a web server.

		The cgi_handlers map in your LocationConfig stores:

		key: a file extension like ".py" or ".php"
		value: the path to the interpreter like "/usr/bin/python3"
		So when a request comes in for /cgi-bin/hello.py, the server looks up ".py" in this map, finds /usr/bin/python3, then spawns that interpreter with the script as an argument and pipes the output back as the HTTP response.

		In a nginx-style config it would look like:


		location /cgi-bin {
			cgi_handler .py /usr/bin/python3;
			cgi_handler .pl /usr/bin/perl;
		}
	*/
	std::map<std::string, std::string>	cgi_handlers;
};

#endif
