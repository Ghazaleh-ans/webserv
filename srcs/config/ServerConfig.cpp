/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ServerConfig.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/15 16:29:36 by gansari           #+#    #+#             */
/*   Updated: 2026/06/01 18:01:19 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "config/ServerConfig.hpp"

ServerConfig::ServerConfig()
	: host("0.0.0.0"), // bind to all interfaces by default
	  port(0), // 0 == "not set", validator rejects this
	  server_names(),
	  client_max_body_size(1048576L), // 1 MiB default -> 1024 x 1024 = 2^20 -> 0x100000
	  error_pages(),
	  locations()
{
}

ServerConfig::~ServerConfig() {}
