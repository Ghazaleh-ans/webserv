/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   RouteDecision.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/21 16:12:38 by gansari           #+#    #+#             */
/*   Updated: 2026/06/03 11:08:39 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "RouteDecision.hpp"

RouteDecision::RouteDecision()
	: kind(KIND_ERROR),
	  fs_path(""),
	  is_directory_request(false),
	  index_file(""),
	  autoindex(false),
	  redirect_code(0),
	  redirect_url(""),
	  error_code(500),
	  effective_body_limit(-1),
	  location(NULL)
{
}
