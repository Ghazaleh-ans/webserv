/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   UploadHandler.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/03 16:57:09 by gansari           #+#    #+#             */
/*   Updated: 2026/06/23 16:17:11 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "UploadHandler.hpp"
#include "MultipartParser.hpp"

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <ctime>

UploadHandler::UploadHandler() {}
UploadHandler::~UploadHandler() {}

// ============================================================
// sanitise_name: strip directory traversal
// ============================================================
// User-controlled input — never trust it. Multipart filenames can be
// "../../etc/passwd" or "C:\Windows\System32\evil.exe". We:
//   1. Take only what's after the last '/' or '\'
//   2. Reject null bytes (file system + C-string boundary issue)
//   3. Reject empty results
//   4. Reject names starting with '.' to keep hidden files out
//
// Result: a filename safe to concat with the store dir.
std::string	UploadHandler::sanitise_name(const std::string& name) const
{
	// Find the last path separator (forward or back slash).
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
		base = "";  // name ended with separator

	// Strip null bytes (defence in depth — open() would truncate at
	// the first null anyway, but let's be explicit).
	std::string clean;
	clean.reserve(base.size());
	for (size_t i = 0; i < base.size(); ++i)
	{
		if (base[i] != '\0')
			clean += base[i];
	}

	// Empty or dot-prefix → use default.
	if (clean.empty() || clean[0] == '.')
		return default_filename();
	return clean;
}

std::string	UploadHandler::default_filename() const
{
	std::stringstream ss;
	ss << "upload_" << std::time(NULL) << ".bin";
	return ss.str();
}

// ============================================================
// write_file: actually create the file
// ============================================================
// Uses std::ofstream — disk I/O is exempt from the poll() rule. We
// open with std::ios::binary so writes are byte-exact (no LF↔CRLF
// translation, though that only matters on Windows).
//
// We DO NOT overwrite existing files: if a file with the same name
// already exists, we append a numeric suffix until we find a free name.
// Most upload services do this; it prevents accidental data loss and
// stops attackers from clobbering pre-existing content.
bool	UploadHandler::write_file(const std::string& store_dir,
								   const std::string& requested_name,
								   const std::string& data,
								   std::string& used_filename_out) const
{
	std::string base_name = sanitise_name(requested_name);

	// Build full path
	std::string dir = store_dir;
	if (!dir.empty() && dir[dir.size() - 1] == '/')
		dir.resize(dir.size() - 1);

	// Find a non-conflicting filename. Try the original first, then
	// add _1, _2, ... up to 1000 attempts. Beyond that, give up
	std::string candidate = base_name;
	for (int attempt = 0; attempt < 1000; ++attempt)
	{
		std::string full = dir + "/" + candidate;

		// Check if file exists
		struct stat st;
		if (stat(full.c_str(), &st) != 0)
		{
			// Doesn't exist -> try to create
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
		// We split the base_name at the last '.' to insert the number
		// before the extension : "report.pdf" -> "report_1.pdf".
		size_t dot = base_name.find_last_of('.');
		std::stringstream next;
		if (dot == std::string::npos)
		{
			next << base_name << "_" << (attempt + 1);
		}
		else
		{
			next << base_name.substr(0, dot)
				<< "_" << (attempt + 1)
				<< base_name.substr(dot);
		}
		candidate = next.str();
	}
	return false;  // gave up after 1000 collisions
}

// Decision tree:
//   - upload_store empty -> 500 (config bug -> the router should not have routed here without it)
//   - body empty -> 400 (nothing to upload)
//   - Content-Type starts with "multipart/form-data" -> multipart path
//   - otherwise -> raw-bytes path, filename from URI tail or default
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

	// Multipart path
	if (!content_type.empty() && content_type.find("multipart/form-data") == 0)
	{
		std::string boundary = MultipartParser::extract_boundary(content_type);
		if (boundary.empty())
		{
			result.status_code = 400;
			result.error_message = "no boundary in Content-Type";
			return result;
		}

		MultipartParser parser;
		if (!parser.parse(req.body, boundary))
		{
			result.status_code = parser.status_code() ? parser.status_code() : 400;
			result.error_message = "multipart parse failed";
			return result;
		}

		// Walk parts; write each one that has a filename (i.e. is a file).
		// Plain form fields (without filename) are silently ignored —
		// we're an upload endpoint, not a form processor.
		size_t files_saved = 0;
		std::string last_used;
		for (size_t i = 0; i < parser.parts().size(); ++i)
		{
			const MultipartPart& part = parser.parts()[i];
			if (part.filename.empty())
				continue;  // plain form field, skip

			std::string used;
			if (!write_file(loc.upload_store, part.filename, part.body, used))
			{
				result.status_code = 500;
				result.error_message =
					"failed to write " + part.filename;
				return result;
			}
			++files_saved;
			last_used = used;
		}

		if (files_saved == 0)
		{
			result.status_code = 400;
			result.error_message = "no file parts in multipart body";
			return result;
		}

		result.status_code = 201;
		result.saved_filename = last_used;
		return result;
	}

	// Raw-bytes path: filename priority:
	//   1. X-Filename header (set by browser form / curl -H)
	//   2. URI tail  e.g. POST /upload/report.pdf
	//   3. default (upload_<timestamp>.bin)
	std::string desired_name = req.header("x-filename");
	if (desired_name.empty())
	{
		size_t slash = req.path.find_last_of('/');
		if (slash != std::string::npos && slash + 1 < req.path.size())
			desired_name = req.path.substr(slash + 1);
	}

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
