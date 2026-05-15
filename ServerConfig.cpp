/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerConfig.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/15 16:29:36 by gansari           #+#    #+#             */
/*   Updated: 2026/05/15 16:29:37 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ServerConfig.hpp"

ServerConfig::ServerConfig()
	: host("0.0.0.0"), // bind to all interfaces by default
	  port(0), // 0 == "not set", validator rejects this
	  server_names(),
	  client_max_body_size(1048576L), // 1 MiB default
	  error_pages(),
	  locations()
{
}

ServerConfig::~ServerConfig() {}
