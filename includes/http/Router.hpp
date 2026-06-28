/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Router.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/21 15:34:53 by gansari           #+#    #+#             */
/*   Updated: 2026/06/24 15:44:24 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef ROUTER_HPP
# define ROUTER_HPP

# include "config/ServerConfig.hpp"
# include "http/HttpRequest.hpp"
# include "http/RouteDecision.hpp"

class Router
{
public:
	Router();
	~Router();


	RouteDecision	route(const HttpRequest& req, const ServerConfig& server) const;

private:
	const LocationConfig*	match_location(const std::string& uri_path, const ServerConfig& server) const;
	bool	method_allowed(const LocationConfig& loc, const std::string& method) const;
	std::string	build_fs_path(const std::string& uri_path, const LocationConfig& loc) const;
	long	effective_body_limit(const LocationConfig& loc, const ServerConfig& server) const;
};

#endif
