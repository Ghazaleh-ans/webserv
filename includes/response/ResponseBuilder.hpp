/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ResponseBuilder.hpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/01 17:43:01 by gansari           #+#    #+#             */
/*   Updated: 2026/06/23 12:37:24 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef RESPONSEBUILDER_HPP
# define RESPONSEBUILDER_HPP

# include <string>
# include "http/HttpRequest.hpp"
# include "config/ServerConfig.hpp"
# include "http/RouteDecision.hpp"
# include "upload/UploadHandler.hpp"

class ResponseBuilder
{
public:
	ResponseBuilder();
	~ResponseBuilder();

	std::string	build(const HttpRequest& req, const RouteDecision& decision, const ServerConfig& server) const;
	std::string	build_error(int code, const ServerConfig& server) const;

private:
	std::string	build_serve(const HttpRequest& req, const RouteDecision& d, const ServerConfig& server) const;
	std::string	build_redirect(const RouteDecision& d) const;

	bool	read_file(const std::string& fs_path, std::string& out) const;
	bool	path_within_root(const std::string& fs_path, const std::string& root) const;
	std::string	list_directory(const std::string& fs_path, const std::string& uri_path) const;

	std::string	make_response(int code, const std::string& content_type, const std::string& body, const std::string& extra_headers) const;

	std::string	reason_phrase(int code) const;

	std::string	handle_delete(const RouteDecision& d, const ServerConfig& server) const;
	std::string	handle_upload(const HttpRequest& req, const RouteDecision& d, const ServerConfig& server) const;

	UploadHandler	_upload_handler;
};

#endif
