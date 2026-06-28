/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ResponseBuilder.cpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/01 17:43:01 by gansari           #+#    #+#             */
/*   Updated: 2026/06/26 13:58:12 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "response/ResponseBuilder.hpp"
#include "response/MimeTypes.hpp"
#include "http/PathUtils.hpp"

#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstring>
#include <vector>

ResponseBuilder::ResponseBuilder() {}
ResponseBuilder::~ResponseBuilder() {}

std::string	ResponseBuilder::reason_phrase(int code) const
{
	switch (code)
	{
		case 200: return "OK";
		case 201: return "Created";
		case 204: return "No Content";
		case 301: return "Moved Permanently";
		case 302: return "Found";
		case 303: return "See Other";
		case 307: return "Temporary Redirect";
		case 308: return "Permanent Redirect";
		case 400: return "Bad Request";
		case 403: return "Forbidden";
		case 404: return "Not Found";
		case 405: return "Method Not Allowed";
		case 413: return "Payload Too Large";
		case 414: return "URI Too Long";
		case 431: return "Request Header Fields Too Large";
		case 500: return "Internal Server Error";
		case 501: return "Not Implemented";
		case 502: return "Bad Gateway";
		case 504: return "Gateway Timeout";
		case 505: return "HTTP Version Not Supported";
		default:  return "Status";
	}
}

std::string	ResponseBuilder::make_response(int code, const std::string& content_type, const std::string& body, const std::string& extra_headers) const
{
	std::stringstream ss;
	ss << "HTTP/1.1 " << code << " " << reason_phrase(code) << "\r\n"
		<< "Content-Type: " << content_type << "\r\n"
		<< "Content-Length: " << body.size() << "\r\n"
		<< "Connection: close\r\n";
	if (!extra_headers.empty())
		ss << extra_headers;
	ss << "\r\n" << body;
	return ss.str();
}

// Traversal protection
bool	ResponseBuilder::path_within_root(const std::string& fs_path, const std::string& root) const
{
	return PathUtils::is_within_root(fs_path, root);
}

bool	ResponseBuilder::read_file(const std::string& fs_path, std::string& out) const
{
	std::ifstream f(fs_path.c_str(), std::ios::binary);
	if (!f.is_open())
		return false;

	std::stringstream buf;
	buf << f.rdbuf();
	out = buf.str();
	return true;
}

std::string	ResponseBuilder::list_directory(const std::string& fs_path, const std::string& uri_path) const
{
	DIR* dir = opendir(fs_path.c_str());
	if (dir == NULL)
		return "";  // caller treats as 500

	// Ensure uri_path ends with '/' for clean concatenation in <a href>
	std::string base = uri_path;
	if (base.empty() || base[base.size() - 1] != '/')
		base += '/';

	std::vector<std::string>	dirs;
	std::vector<std::string>	files;

	struct dirent*	entry;
	while ((entry = readdir(dir)) != NULL)
	{
		std::string name = entry->d_name;
		if (name == "." || name == "..")
			continue;

		std::string child_path = fs_path;
		if (!child_path.empty() && child_path[child_path.size() - 1] != '/')
			child_path += '/';
		child_path += name;

		struct stat st;
		if (stat(child_path.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
			dirs.push_back(name);
		else
			files.push_back(name);
	}
	closedir(dir);

	for (size_t i = 0; i + 1 < dirs.size(); ++i)
		for (size_t j = 0; j + 1 < dirs.size() - i; ++j)
			if (dirs[j] > dirs[j + 1])
				dirs[j].swap(dirs[j + 1]);
	for (size_t i = 0; i + 1 < files.size(); ++i)
		for (size_t j = 0; j + 1 < files.size() - i; ++j)
			if (files[j] > files[j + 1])
				files[j].swap(files[j + 1]);

	std::stringstream html;
	html << "<!DOCTYPE html>\r\n"
		<< "<html><head><title>Index of " << base << "</title></head>\r\n"
		<< "<body><h1>Index of " << base << "</h1>\r\n<ul>\r\n";

	for (size_t i = 0; i < dirs.size(); ++i)
		html << "<li><a href=\"" << base << dirs[i] << "/\">"
			<< dirs[i] << "/</a></li>\r\n";
	for (size_t i = 0; i < files.size(); ++i)
		html << "<li><a href=\"" << base << files[i] << "\">"
			<< files[i] << "</a></li>\r\n";

	html << "</ul></body></html>\r\n";
	return html.str();
}

// We refuse to delete directories
std::string	ResponseBuilder::handle_delete(const RouteDecision& d, const ServerConfig& server) const
{
	const std::string& root = d.location ? d.location->root : std::string();

	if (!path_within_root(d.fs_path, root))
		return build_error(403, server);

	struct stat st;
	if (stat(d.fs_path.c_str(), &st) != 0)
		return build_error(404, server);
	if (S_ISDIR(st.st_mode))
		return build_error(403, server);

	if (std::remove(d.fs_path.c_str()) != 0)
		return build_error(500, server);

	// 204 means "success, no body" -> Content-Length: 0 mandatory
	std::stringstream ss;
	ss << "HTTP/1.1 204 No Content\r\n"
		<< "Content-Length: 0\r\n"
		<< "Connection: close\r\n\r\n";
	return ss.str();
}

std::string	ResponseBuilder::handle_upload(const HttpRequest& req, const RouteDecision& d, const ServerConfig& server) const
{
	if (d.location == NULL)
		return build_error(500, server);

	UploadResult result = _upload_handler.handle(req, *d.location);

	if (result.status_code != 201)
		return build_error(result.status_code, server);

	std::stringstream body;
	body << "{\r\n"
		<< "  \"status\": \"ok\",\r\n"
		<< "  \"filename\": \"" << result.saved_filename << "\",\r\n"
		<< "  \"bytes\": " << req.body.size() << "\r\n"
		<< "}\r\n";

	std::stringstream extra;
	extra << "Location: " << req.path;
	if (!req.path.empty() && req.path[req.path.size() - 1] != '/')
		extra << "/";
	extra << result.saved_filename << "\r\n";

	return make_response(201, "application/json", body.str(), extra.str());
}

std::string	ResponseBuilder::build_redirect(const RouteDecision& d) const
{
	std::stringstream body;
	body << "<html><body><h1>" << d.redirect_code << " "
		<< reason_phrase(d.redirect_code) << "</h1>"
		<< "<p>See <a href=\"" << d.redirect_url << "\">"
		<< d.redirect_url << "</a></p></body></html>\r\n";

	std::stringstream extra;
	extra << "Location: " << d.redirect_url << "\r\n";

	return make_response(d.redirect_code, "text/html; charset=utf-8", body.str(), extra.str());
}

// Steps:
//   1. DELETE requests -> handle_delete
//   2. Path traversal check (refuse 403 if escaping root)
//   3. stat() the resolved path
//      - missing -> 404
//      - directory:
//          a. try `<fs_path>/<index_file>` if index is set
//          b. else if autoindex -> list_directory
//          c. else -> 403
//      - regular file -> read it, send with MIME type
//   4. Anything that fails along the way -> error
std::string	ResponseBuilder::build_serve(const HttpRequest& req, const RouteDecision& d, const ServerConfig& server) const
{
	if (req.method == "DELETE")
		return handle_delete(d, server);
	// POST with upload_store configured -> upload handler
	// Without upload_store, a POST to a normal location just falls through to the GET-like serve path
	if (req.method == "POST" && d.location != NULL && !d.location->upload_store.empty())
		return handle_upload(req, d, server);

	const std::string& root = d.location ? d.location->root : std::string();

	// Traversal protection: if the resolved path escapes the location's
	// root, it's an attack or a bug -> 403
	if (!path_within_root(d.fs_path, root))
		return build_error(403, server);

	// Check if the file exist
	struct stat st;
	if (stat(d.fs_path.c_str(), &st) != 0)
		return build_error(404, server);

	// Directory case
	if (S_ISDIR(st.st_mode))
	{
		// Browser convention: GET /foo (a directory) should redirect to
		// /foo/ so relative links resolve correctly
		// We do it only when the URI doesn't already end in '/'
		if (!req.path.empty() && req.path[req.path.size() - 1] != '/')
		{
			RouteDecision fake = d;
			fake.kind = RouteDecision::KIND_REDIRECT;
			fake.redirect_code = 301;
			fake.redirect_url = req.path + "/";
			return build_redirect(fake);
		}

		// Try index file
		if (!d.index_file.empty())
		{
			std::string idx_path = d.fs_path;
			if (!idx_path.empty() && idx_path[idx_path.size() - 1] != '/')
				idx_path += '/';
			idx_path += d.index_file;

			struct stat idx_st;
			if (stat(idx_path.c_str(), &idx_st) == 0 && S_ISREG(idx_st.st_mode))
			{
				std::string body;
				if (!read_file(idx_path, body))
					return build_error(500, server);
				return make_response(200, MimeTypes::content_type_for(idx_path), body, "");
			}
		}

		// Autoindex if enabled
		if (d.autoindex)
		{
			std::string body = list_directory(d.fs_path, req.path);
			if (body.empty())
				return build_error(500, server);
			return make_response(200, "text/html; charset=utf-8", body, "");
		}

		// Directory access without index and without autoindex -> 403.
		return build_error(403, server);
	}

	// Regular file case
	if (!S_ISREG(st.st_mode))
	{
		// Not a directory, not a regular file (socket, fifo, block device)
		return build_error(403, server);
	}

	std::string body;
	if (!read_file(d.fs_path, body))
		return build_error(500, server);

	return make_response(200, MimeTypes::content_type_for(d.fs_path), body, "");
}

std::string	ResponseBuilder::build_error(int code, const ServerConfig& server) const
{
	std::map<int, std::string>::const_iterator it = server.error_pages.find(code);

	if (it != server.error_pages.end() && !server.locations.empty())
	{
		// Resolve the configured error-page path against the first
		// location's root.
		const std::string& root = server.locations[0].root;
		std::string fs_path = root;
		const std::string& page = it->second;
		if (!fs_path.empty() && fs_path[fs_path.size() - 1] == '/' && !page.empty() && page[0] == '/')
			fs_path.resize(fs_path.size() - 1);
		fs_path += page;

		std::string body;
		if (read_file(fs_path, body))
			return make_response(code, MimeTypes::content_type_for(fs_path), body, "");
		// Custom page failed to load -> fall through to default
	}

	// Default: try www/errors/{code}.html relative to first location's root
	if (!server.locations.empty())
	{
		std::ostringstream default_path;
		default_path << server.locations[0].root << "/errors/" << code << ".html";
		std::string body;
		if (read_file(default_path.str(), body))
			return make_response(code, "text/html; charset=utf-8", body, "");
	}

	// Last-resort inline fallback
	std::ostringstream body;
	body << "<!DOCTYPE html>\r\n"
		<< "<html><head><title>" << code << " " << reason_phrase(code)
		<< "</title></head>\r\n"
		<< "<body><h1>" << code << " " << reason_phrase(code) << "</h1>\r\n"
		<< "<p>webserv</p></body></html>\r\n";

	return make_response(code, "text/html; charset=utf-8", body.str(), "");
}

std::string	ResponseBuilder::build(const HttpRequest& req, const RouteDecision& d, const ServerConfig& server) const
{
	switch (d.kind)
	{
		case RouteDecision::KIND_SERVE:
			return build_serve(req, d, server);
		case RouteDecision::KIND_REDIRECT:
			return build_redirect(d);
		case RouteDecision::KIND_ERROR:
			return build_error(d.error_code, server);
		case RouteDecision::KIND_CGI:
			return build_error(500, server); // This will never happen
		default:
			return build_error(500, server);
	}
}
