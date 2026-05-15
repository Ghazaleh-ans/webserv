/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/15 16:28:56 by gansari           #+#    #+#             */
/*   Updated: 2026/05/15 16:28:57 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Client.hpp"
#include "SocketUtils.hpp"

#include <sys/socket.h>
#include <unistd.h>
#include <sstream>
#include <ctime>

// Read this many bytes per recv() call. Bigger = fewer syscalls per
// request but more memory per client; 8 KiB is the traditional sweet spot.
static const size_t	RECV_CHUNK = 8192;

Client::Client(int fd, const ServerConfig& config)
	: _fd(fd),
	  _config(&config),
	  _in_buffer(),
	  _out_buffer(),
	  _last_active(std::time(NULL)),
	  _should_close(false),
	  _response_built(false)
{
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

	// One recv() per poll event. Don't loop here — if there's more
	// data, poll() will tell us next iteration. Looping in a single
	// poll-event handler is a classic way to starve other clients.
	ssize_t n = recv(_fd, buf, sizeof(buf), 0);

	// recv() return semantics — IMPORTANT, this is where the subject
	// rule "no errno after read/write" forces us to be careful:
	//   n > 0  : got n bytes
	//   n == 0 : peer closed cleanly (FIN received)
	//   n < 0  : either WOULDBLOCK (poll lied / spurious wakeup) or a
	//            real error. We can't distinguish without errno, so
	//            we treat all <0 as "give up on this connection".
	//            This matches the subject's rules exactly.
	if (n == 0)
		return false;  // peer disconnected
	if (n < 0)
		return false;  // assume fatal, drop the client

	_in_buffer.append(buf, static_cast<size_t>(n));
	touch();

	// Skeleton: as soon as we see the end of an HTTP request (the
	// blank line "\r\n\r\n"), build a canned response. Real parsing
	// arrives in Module 3.
	if (!_response_built && _in_buffer.find("\r\n\r\n") != std::string::npos)
		try_build_response();

	// Cap inbound buffer at the configured limit, even before parsing.
	// Otherwise a malicious client could feed us gigabytes before
	// Module 3 lands.
	if (_in_buffer.size() > static_cast<size_t>(_config->client_max_body_size)
		+ 8192)  // +8K to allow generous headers
	{
		return false;
	}

	return true;
}

bool	Client::on_writable()
{
	if (_out_buffer.empty())
		return true;  // nothing to do; poll shouldn't have called us, but fine

	// Same "one syscall per poll event" rule as on_readable.
	ssize_t n = send(_fd, _out_buffer.data(), _out_buffer.size(), 0);
	if (n <= 0)
		return false;  // treat both errors and 0 as fatal (no errno check)

	// Slide the buffer: drop the bytes we successfully sent.
	// std::string::erase is O(n) — for a project server it's fine;
	// optimizing this would use an offset+data() trick.
	_out_buffer.erase(0, static_cast<size_t>(n));
	touch();

	// If we drained the whole response (skeleton: HTTP/1.0 "Connection:
	// close" semantics), close after sending.
	if (_out_buffer.empty() && _response_built)
		_should_close = true;

	return true;
}

void	Client::try_build_response()
{
	_response_built = true;

	// Hardcoded HTTP/1.1 200 response. Note CRLF, not bare LF — HTTP
	// requires \r\n line endings. Many browsers tolerate \n, but our
	// goal is correctness, not just luck.
	//
	// We dump a few fields from _config so peers (and you, during
	// defense) can SEE that the parser's data actually reached this
	// Client. Real Module 3 will replace this with proper routing.
	std::stringstream body;
	body << "<!DOCTYPE html>\r\n"
		<< "<html><head><title>webserv</title></head>\r\n"
		<< "<body><h1>webserv is alive</h1>\r\n"
		<< "<p>Module 2 skeleton: poll() loop accepting connections.</p>\r\n"
		<< "<h2>Config seen by this Client:</h2>\r\n"
		<< "<ul>\r\n"
		<< "  <li>host: " << _config->host << "</li>\r\n"
		<< "  <li>port: " << _config->port << "</li>\r\n"
		<< "  <li>client_max_body_size: "
		<< _config->client_max_body_size << "</li>\r\n"
		<< "  <li>locations configured: "
		<< _config->locations.size() << "</li>\r\n"
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
