/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   UploadHandler.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/03 16:57:03 by gansari           #+#    #+#             */
/*   Updated: 2026/07/01 17:29:15 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef UPLOADHANDLER_HPP
# define UPLOADHANDLER_HPP

# include <string>
# include "http/HttpRequest.hpp"
# include "config/LocationConfig.hpp"

struct UploadResult
{
	int			status_code; // 201 on success, 4xx/5xx on failure
	std::string	saved_filename; // base filename used on disk (success only)
	std::string	error_message; // human-readable description (any)
};

class UploadHandler
{
public:
	UploadHandler();
	~UploadHandler();

	UploadResult	handle(const HttpRequest& req, const LocationConfig& loc) const;

private:
	bool	write_file(const std::string& store_dir, const std::string& requested_name, const std::string& data, std::string& used_filename_out) const;

	std::string	sanitise_name(const std::string& name) const;
	std::string	default_filename() const;
};

#endif
