/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   UploadHandler.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/03 16:57:03 by gansari           #+#    #+#             */
/*   Updated: 2026/06/04 11:28:28 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef UPLOADHANDLER_HPP
# define UPLOADHANDLER_HPP

# include <string>
# include "HttpRequest.hpp"
# include "LocationConfig.hpp"

struct UploadResult
{
	int			status_code;        // 201 on success, 4xx/5xx on failure
	std::string	saved_filename;     // base filename used on disk (success only)
	std::string	error_message;      // human-readable description (any)
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

	// Process the upload -> `store_dir` is the configured upload_store
	// from the matched LocationConfig
	// Returns a populated result, Never throws
	UploadResult	handle(const HttpRequest& req,
						   const LocationConfig& loc) const;

private:
	// Write `data` to `store_dir/safe_filename`
	// Returns true on success
	// Path is sanitised: any '/' or null bytes in `requested_name` stripped
	// We won't create subdirectories -> attacker-supplied names must not escape store_dir
	bool	write_file(const std::string& store_dir,
					   const std::string& requested_name,
					   const std::string& data,
					   std::string& used_filename_out) const;

	// Strip any directory components from a name
	// "foo/bar.txt" -> "bar.txt"
	// "../../etc/passwd" -> "passwd"
	//  Empty input -> "upload" (a default)
	std::string	sanitise_name(const std::string& name) const;

	// Construct a fallback filename if none was supplied -> Uses time(NULL)
	// to keep names unique across invocations -> Format: "upload_<seconds>.bin"
	std::string	default_filename() const;
};

#endif
