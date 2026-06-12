/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequest.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/20 12:55:47 by gansari           #+#    #+#             */
/*   Updated: 2026/06/01 16:06:49 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef HTTPREQUEST_HPP
# define HTTPREQUEST_HPP

# include <string>
# include <map>

// Case-insensitive comparator for header names.
struct CaseInsensitiveLess
{
	bool	operator()(const std::string& a, const std::string& b) const;
};

class HttpRequest
{
public:
	HttpRequest();
	~HttpRequest();

	void	reset();

	std::string	method;       // "GET", "POST", "DELETE"
	std::string	uri;          // raw URI before any decoding, e.g. "/foo?bar=baz"
	std::string	path;         // URI minus query string, e.g. "/foo"
	std::string	query;        // everything after ?, e.g. "bar=baz" (or empty)
	std::string	version;      // "HTTP/1.1"

	// Headers stored with case-insensitive lookup.
	std::map<std::string, std::string, CaseInsensitiveLess>	headers;

	// Body bytes. For chunked transfers this holds the un-chunked payload.
	std::string	body;

	// Return empty string if the header is not present
	std::string	header(const std::string& name) const;
	bool		has_header(const std::string& name) const;
};

#endif
