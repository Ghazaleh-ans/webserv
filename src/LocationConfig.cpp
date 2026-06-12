/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   LocationConfig.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/15 16:29:19 by gansari           #+#    #+#             */
/*   Updated: 2026/05/15 16:29:20 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "LocationConfig.hpp"

LocationConfig::LocationConfig()
	: path(""),
	  root(""),
	  index(""),
	  methods(),
	  has_redirect(false),
	  redirect_code(0),
	  redirect_url(""),
	  autoindex(false),
	  client_max_body_size(-1), // -1 == "inherit from server"
	  upload_store(""),
	  cgi_handlers()
{
}

LocationConfig::~LocationConfig() {}
