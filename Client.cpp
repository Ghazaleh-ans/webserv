/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/15 16:28:56 by gansari           #+#    #+#             */
/*   Updated: 2026/05/21 15:09:16 by gansari          ###   ########.fr       */
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
	  _out_buffer(),
	  _last_active(std::time(NULL)),
	  _should_close(false),
	  _response_built(false)
{
	// Tell the parser about this server's body-size limit so it can
	// short-circuit oversized uploads at header-parse time (413
	// Payload Too Large) instead of waiting for the bytes to arrive.
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

	// One recv() per poll event. See Module 2 walkthrough for the
	// "don't loop here" reasoning — it would starve other clients
	// and tempt us to check errno.
	ssize_t n = recv(_fd, buf, sizeof(buf), 0);
	if (n == 0)
		return false;  // clean disconnect from peer
	if (n < 0)
		return false;  // any error treated as fatal (no errno check)

	touch();

	// Feed everything we read into the parser. The parser handles
	// partial input transparently — bytes that don't complete a line
	// stay in its internal buffer.
	if (_response_built)
	{
		// Pipelined request? We don't support keep-alive yet, so any
		// data arriving after we've built a response is unexpected.
		// Drop quietly.
		return true;
	}

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

	// Module 3 still uses a diagnostic response that echoes what the
	// parser saw. This is the test bench — once we can SEE the
	// parser working from a browser, we trust it and replace this
	// with the real router (Module 4).
	const HttpRequest& req = _parser.request();

	std::stringstream body;
	body << "<!DOCTYPE html>\r\n"
		<< "<html><head><title>webserv</title></head>\r\n"
		<< "<body><h1>Request received</h1>\r\n"
		<< "<h2>Parsed request:</h2>\r\n"
		<< "<ul>\r\n"
		<< "  <li><b>method:</b> " << req.method << "</li>\r\n"
		<< "  <li><b>path:</b> " << req.path << "</li>\r\n"
		<< "  <li><b>query:</b> " << req.query << "</li>\r\n"
		<< "  <li><b>version:</b> " << req.version << "</li>\r\n"
		<< "  <li><b>headers:</b> " << req.headers.size() << "</li>\r\n"
		<< "  <li><b>body size:</b> " << req.body.size() << " bytes</li>\r\n"
		<< "  <li><b>body:</b> " << req.body << "</li>\r\n"
		<< "</ul>\r\n"
		<< "<h2>Server config seen:</h2>\r\n"
		<< "<ul>\r\n"
		<< "  <li>host:port: " << _config->host
		<< ":" << _config->port << "</li>\r\n"
		<< "  <li>client_max_body_size: "
		<< _config->client_max_body_size << "</li>\r\n"
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

// Status-line text for the few error codes we emit at this stage.
// Real Module 5 will produce richer error pages.
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