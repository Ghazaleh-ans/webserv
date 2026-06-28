/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   UploadHandler.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/03 16:57:09 by gansari           #+#    #+#             */
/*   Updated: 2026/06/26 12:20:04 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "upload/UploadHandler.hpp"

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <ctime>

UploadHandler::UploadHandler() {}
UploadHandler::~UploadHandler() {}

//   1. Take only what's after the last '/' or '\'
//   2. Strip control characters (NUL, CR, LF, ...) -> they corrupt the on-disk name and, if echoed into the Location: header, enable response splitting
//   3. Reject empty results
//   4. Reject names starting with '.' to keep hidden files out
// Result: a filename safe to concat with the store dir and put in a header
std::string	UploadHandler::sanitise_name(const std::string& name) const
{
	// Find the last path separator (forward or back slash)
	size_t last_sep = name.size();
	for (size_t i = 0; i < name.size(); ++i)
	{
		if (name[i] == '/' || name[i] == '\\')
			last_sep = i;
	}

	std::string base;
	if (last_sep == name.size())
		base = name;
	else if (last_sep + 1 < name.size())
		base = name.substr(last_sep + 1);
	else
		base = ""; // name ended with separator

	std::string clean;
	clean.reserve(base.size());
	for (size_t i = 0; i < base.size(); ++i)
	{
		unsigned char ch = static_cast<unsigned char>(base[i]);
		if (ch >= 0x20 && ch != 0x7F)
			clean += base[i];
	}

	if (clean.empty() || clean[0] == '.')
		return default_filename("");
	return clean;
}

std::string	UploadHandler::default_filename(const std::string& content_type) const
{
	std::string ext = ".bin";
	if (content_type.find("text/") == 0 || content_type.find("application/x-www-form-urlencoded") == 0)
		ext = ".txt";
	else if (content_type.find("application/json") == 0)
		ext = ".json";

	std::stringstream ss;
	ss << "upload_" << std::time(NULL) << ext;
	return ss.str();
}

bool	UploadHandler::write_file(const std::string& store_dir, const std::string& requested_name, const std::string& data, std::string& used_filename_out) const
{
	std::string base_name = sanitise_name(requested_name);

	// Build full path
	std::string dir = store_dir;
	if (!dir.empty() && dir[dir.size() - 1] == '/')
		dir.resize(dir.size() - 1);

	std::string candidate = base_name;
	for (int attempt = 0; attempt < 1000; ++attempt)
	{
		std::string full = dir + "/" + candidate;

		// Check if file exists
		struct stat st;
		if (stat(full.c_str(), &st) != 0)
		{
			// Doesn't exist -> try to create
			// Write files exactly as-is -> std::ios::binary
			// Open the file for writing -> std::ios::out
			std::ofstream f(full.c_str(), std::ios::binary | std::ios::out);
			if (!f.is_open())
				return false;
			f.write(data.data(), static_cast<std::streamsize>(data.size()));
			if (!f.good())
				return false;
			used_filename_out = candidate;
			return true;
		}

		// Already exists -> try base_name_<n>
		size_t dot = base_name.find_last_of('.');
		std::stringstream next;
		if (dot == std::string::npos)
		{
			next << base_name << "_" << (attempt + 1);
		}
		else
		{
			next << base_name.substr(0, dot) << "_" << (attempt + 1) << base_name.substr(dot);
		}
		candidate = next.str();
	}
	return false; // gave up after 1000 collisions
}

// Writes the raw request body to upload_store.
// Filename priority:
//   1. X-Filename header (e.g. curl -H "X-Filename: report.pdf")
//   2. URI tail (e.g. POST /upload/report.pdf)
//   3. Generated name based on Content-Type (upload_<timestamp>.txt/.json/.bin)
UploadResult	UploadHandler::handle(const HttpRequest& req, const LocationConfig& loc) const
{
	UploadResult result;
	result.status_code = 500;

	if (loc.upload_store.empty())
	{
		result.error_message = "upload_store not configured";
		return result;
	}

	if (req.body.empty())
	{
		result.status_code = 400;
		result.error_message = "empty body";
		return result;
	}

	struct stat st;
	if (stat(loc.upload_store.c_str(), &st) != 0 || !S_ISDIR(st.st_mode))
	{
		result.status_code = 500;
		result.error_message = "upload_store does not exist";
		return result;
	}

	std::string content_type = req.header("content-type");

	std::string desired_name = req.header("x-filename");
	if (desired_name.empty())
	{
		size_t slash = req.path.find_last_of('/');
		if (slash != std::string::npos && slash + 1 < req.path.size())
			desired_name = req.path.substr(slash + 1);
	}
	if (desired_name.empty())
		desired_name = default_filename(content_type);

	std::string used;
	if (!write_file(loc.upload_store, desired_name, req.body, used))
	{
		result.status_code = 500;
		result.error_message = "failed to write upload";
		return result;
	}

	result.status_code = 201;
	result.saved_filename = used;
	return result;
}
