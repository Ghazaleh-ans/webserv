/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/15 16:28:56 by gansari           #+#    #+#             */
/*   Updated: 2026/05/22 14:21:23 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Client.hpp"
#include "SocketUtils.hpp"

#include <sys/socket.h>
#include <unistd.h>
#include <sstream>
#include <ctime>

// One recv() reads up to this many bytes per poll event. 8 KiB is the
// traditional sweet spot: large enough that small requests arrive in
// one syscall, small enough that we don't hog the loop on a single client.
static const size_t	RECV_CHUNK = 8192;

Client::Client(int fd, const ServerConfig& config)
	: _fd(fd),
	  _config(&config),
	  _parser(),
	  _router(),
	  _out_buffer(),
	  _last_active(std::time(NULL)),
	  _should_close(false),
	  _response_built(false)
{
	if (_config->client_max_body_size > 0)
		_parser.set_max_body_size(
			static_cast<size_t>(_config->client_max_body_size));
}

Client::~Client()
{
	SocketUtils::safe_close(_fd);
}

int						Client::fd() const           { return _fd; }
const ServerConfig&		Client::config() const       { return *_config; }
bool					Client::has_data_to_send() const { return !_out_buffer.empty(); }
bool					Client::should_close() const { return _should_close; }
std::time_t				Client::last_active() const  { return _last_active; }
void					Client::touch()              { _last_active = std::time(NULL); }

bool	Client::on_readable()
{
	char buf[RECV_CHUNK];

	// don't loop here -> it would starve other clients + errno check
	ssize_t n = recv(_fd, buf, sizeof(buf), 0);
	if (n == 0)
		return false; // clean disconnect from peer
	if (n < 0)
		return false; // any error

	touch();

	// Feed everything we read into the parser
	// bytes that don't complete a line stay in its internal buffer
	if (_response_built)
		return true;

	HttpRequestParser::State st = _parser.feed(buf, static_cast<size_t>(n));

	if (st == HttpRequestParser::STATE_ERROR)
	{
		build_error_response(_parser.status_code());
		return true;
	}
	if (st == HttpRequestParser::STATE_DONE)
	{
		build_response();
		return true;
	}
	// Otherwise: still parsing, wait for more bytes.
	return true;
}

bool	Client::on_writable()
{
	if (_out_buffer.empty())
		return true;

	ssize_t n = send(_fd, _out_buffer.data(), _out_buffer.size(), 0);
	if (n <= 0)
		return false;

	_out_buffer.erase(0, static_cast<size_t>(n));
	touch();

	if (_out_buffer.empty() && _response_built)
		_should_close = true;

	return true;
}

void	Client::build_response()
{
	_response_built = true;

	const HttpRequest& req = _parser.request();
	RouteDecision d = _router.route(req, *_config);

	if (d.kind == RouteDecision::KIND_REDIRECT)
	{
		std::stringstream body;
		body << "<html><body><h1>" << d.redirect_code
			<< " Redirect</h1><p>See <a href=\""
			<< d.redirect_url << "\">"
			<< d.redirect_url << "</a></p></body></html>\r\n";
		const std::string body_str = body.str();

		std::stringstream resp;
		resp << "HTTP/1.1 " << d.redirect_code << " ";
		if (d.redirect_code == 301)      resp << "Moved Permanently";
		else if (d.redirect_code == 302) resp << "Found";
		else if (d.redirect_code == 303) resp << "See Other";
		else if (d.redirect_code == 307) resp << "Temporary Redirect";
		else if (d.redirect_code == 308) resp << "Permanent Redirect";
		else                              resp << "Redirect";
		resp << "\r\n"
			<< "Location: " << d.redirect_url << "\r\n"
			<< "Content-Type: text/html\r\n"
			<< "Content-Length: " << body_str.size() << "\r\n"
			<< "Connection: close\r\n"
			<< "\r\n"
			<< body_str;
		_out_buffer = resp.str();
		return;
	}

	if (d.kind == RouteDecision::KIND_ERROR)
	{
		build_error_response(d.error_code);
		return;
	}

	std::stringstream body;
	body << "<!DOCTYPE html>\r\n"
		<< "<html><head><title>webserv routed</title></head>\r\n"
		<< "<body><h1>Routing decision</h1>\r\n"
		<< "<h2>Request</h2><ul>\r\n"
		<< "  <li><b>method:</b> " << req.method << "</li>\r\n"
		<< "  <li><b>uri path:</b> " << req.path << "</li>\r\n"
		<< "</ul>\r\n"
		<< "<h2>Matched location</h2><ul>\r\n"
		<< "  <li><b>location path:</b> "
		<< (d.location ? d.location->path : "(none)") << "</li>\r\n"
		<< "  <li><b>root:</b> "
		<< (d.location ? d.location->root : "(none)") << "</li>\r\n"
		<< "</ul>\r\n"
		<< "<h2>Resolved</h2><ul>\r\n"
		<< "  <li><b>fs_path:</b> " << d.fs_path << "</li>\r\n"
		<< "  <li><b>directory request:</b> "
		<< (d.is_directory_request ? "yes" : "no") << "</li>\r\n"
		<< "  <li><b>index file:</b> "
		<< (d.index_file.empty() ? "(none)" : d.index_file) << "</li>\r\n"
		<< "  <li><b>autoindex:</b> "
		<< (d.autoindex ? "on" : "off") << "</li>\r\n"
		<< "  <li><b>effective body limit:</b> "
		<< d.effective_body_limit << "</li>\r\n"
		<< "</ul>\r\n"
		<< "</body></html>\r\n";

	const std::string body_str = body.str();
	std::stringstream resp;
	resp << "HTTP/1.1 200 OK\r\n"
		<< "Content-Type: text/html\r\n"
		<< "Content-Length: " << body_str.size() << "\r\n"
		<< "Connection: close\r\n"
		<< "\r\n"
		<< body_str;
	_out_buffer = resp.str();
}

static std::string	reason_for(int code)
{
	switch (code)
	{
		case 400: return "Bad Request";
		case 413: return "Payload Too Large";
		case 414: return "URI Too Long";
		case 431: return "Request Header Fields Too Large";
		case 500: return "Internal Server Error";
		case 501: return "Not Implemented";
		case 505: return "HTTP Version Not Supported";
		default:  return "Error";
	}
}

void	Client::build_error_response(int code)
{
	_response_built = true;

	const std::string reason = reason_for(code);

	std::stringstream body;
	body << "<!DOCTYPE html>\r\n"
		<< "<html><head><title>" << code << " " << reason << "</title></head>\r\n"
		<< "<body><h1>" << code << " " << reason << "</h1></body></html>\r\n";
	const std::string body_str = body.str();

	std::stringstream resp;
	resp << "HTTP/1.1 " << code << " " << reason << "\r\n"
		<< "Content-Type: text/html\r\n"
		<< "Content-Length: " << body_str.size() << "\r\n"
		<< "Connection: close\r\n"
		<< "\r\n"
		<< body_str;

	_out_buffer = resp.str();
}