/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   RouteDecision.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/21 16:12:38 by gansari           #+#    #+#             */
/*   Updated: 2026/05/21 16:12:39 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "RouteDecision.hpp"

// Safe defaults: a default-constructed RouteDecision is a 500 error
// pointing at nothing. The Router must overwrite this with a real
// verdict before returning.
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
