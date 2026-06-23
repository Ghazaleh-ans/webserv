/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   UploadHandler.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/03 16:57:03 by gansari           #+#    #+#             */
/*   Updated: 2026/06/23 16:10:07 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef UPLOADHANDLER_HPP
# define UPLOADHANDLER_HPP

# include <string>
# include "HttpRequest.hpp"
# include "LocationConfig.hpp"

struct UploadResult
{
	int			status_code; // 201 on success, 4xx/5xx on failure
	std::string	saved_filename; // base filename used on disk (success only)
	std::string	error_message; // human-readable description (any)
};

// Stateless: handle a POST whose route has upload_store configured
//
// Supports two content types:
//   1. multipart/form-data -> extracts each file part, writes one file per
//      named file part. Returns 201 on success.
//   2. anything else -> writes the raw body to a single file. The filename
//      is taken from the URI's last path segment (if any) or a generated
//      name. This catches `curl --data-binary @file.bin -X POST` and
//      similar tooling.
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
