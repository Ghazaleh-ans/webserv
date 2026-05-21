/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequest.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/20 13:05:16 by gansari           #+#    #+#             */
/*   Updated: 2026/05/21 12:29:59 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "HttpRequest.hpp"
#include <cctype>

bool	CaseInsensitiveLess::operator()(const std::string& a,
									const std::string& b) const
{
	std::string::const_iterator ai = a.begin();
	std::string::const_iterator bi = b.begin();
	while (ai != a.end() && bi != b.end())
	{
		unsigned char ca = static_cast<unsigned char>(*ai);
		unsigned char cb = static_cast<unsigned char>(*bi);
		ca = static_cast<unsigned char>(std::tolower(ca));
		cb = static_cast<unsigned char>(std::tolower(cb));
		if (ca != cb)
			return ca < cb;
		++ai;
		++bi;
	}
	return a.size() < b.size();
}

HttpRequest::HttpRequest() { reset(); }
HttpRequest::~HttpRequest() {}

void	HttpRequest::reset()
{
	method.clear();
	uri.clear();
	path.clear();
	query.clear();
	version.clear();
	headers.clear();
	body.clear();
}

std::string	HttpRequest::header(const std::string& name) const
{
	std::map<std::string, std::string, CaseInsensitiveLess>::const_iterator it
		= headers.find(name);
	if (it == headers.end())
		return "";
	return it->second;
}

bool	HttpRequest::has_header(const std::string& name) const
{
	return headers.find(name) != headers.end();
}
