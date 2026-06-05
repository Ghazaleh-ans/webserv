/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ResponseBuilder.hpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/01 17:43:01 by gansari           #+#    #+#             */
/*   Updated: 2026/06/03 17:58:37 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef RESPONSEBUILDER_HPP
# define RESPONSEBUILDER_HPP

# include <string>
# include "HttpRequest.hpp"
# include "ServerConfig.hpp"
# include "RouteDecision.hpp"
# include "UploadHandler.hpp"

class ResponseBuilder
{
public:
	ResponseBuilder();
	~ResponseBuilder();

	// Public entry point: produce a response from the decision
	// Always returns valid HTTP bytes — worst case, a 500 with a
	// minimal HTML page. Never throws.
	std::string	build(const HttpRequest& req,
					   const RouteDecision& decision,
					   const ServerConfig& server) const;

	// Build an error response directly (used by Client when the
	// parser itself failed, before routing was possible).
	// Honours ServerConfig::error_pages if a custom page is configured
	// AND readable. Otherwise emits a built-in HTML page.
	std::string	build_error(int code,
							 const ServerConfig& server) const;

private:
	std::string	build_serve(const HttpRequest& req,
							const RouteDecision& d,
							const ServerConfig& server) const;
	std::string	build_redirect(const RouteDecision& d) const;

	bool	read_file(const std::string& fs_path, std::string& out) const;
	bool	path_within_root(const std::string& fs_path,
							  const std::string& root) const;
	std::string	list_directory(const std::string& fs_path,
							   const std::string& uri_path) const;

	std::string	make_response(int code,
							  const std::string& content_type,
							  const std::string& body,
							  const std::string& extra_headers) const;

	std::string	reason_phrase(int code) const;

	std::string	handle_delete(const RouteDecision& d,
							  const ServerConfig& server) const;
	std::string	handle_upload(const HttpRequest& req,
							  const RouteDecision& d,
							  const ServerConfig& server) const;

	UploadHandler	_upload_handler;
};

#endif
