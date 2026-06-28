/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/15 16:28:56 by gansari           #+#    #+#             */
/*   Updated: 2026/06/26 10:49:09 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "core/Client.hpp"
#include "network/SocketUtils.hpp"

#include <sys/socket.h>
#include <unistd.h>
#include <sstream>
#include <ctime>

// One recv() reads up to this many bytes per poll event. 8 KiB is the
// traditional sweet spot: large enough that small requests arrive in
// one syscall, small enough that we don't hog the loop on a single client
static const size_t	RECV_CHUNK = 8192;

Client::Client(int fd, const ServerConfig& config)
	: _fd(fd),
	  _config(&config),
	  _parser(),
	  _router(),
	  _response_builder(),
	  _out_buffer(),
	  _last_active(std::time(NULL)),
	  _should_close(false),
	  _response_built(false),
	  _cgi(NULL)
{
	// The parser enforces a body-size cap *while* it reads bytes, long before
	// routing has run -> at this point we don't yet know which location the
	// request targets. A location may *raise* the limit above the server
	// default, so if we capped the parser at the server limit it would reject
	// (413) a body that the matched location would actually accept.
	//
	// Fix: cap the parser at the largest limit any location in this server
	// could permit (the server default or any location override, whichever is
	// bigger). This is a coarse safety bound; the precise per-location limit is
	// still enforced after routing in ResponseBuilder via effective_body_limit.
	long max_body = _config->client_max_body_size;
	for (size_t i = 0; i < _config->locations.size(); ++i)
	{
		if (_config->locations[i].client_max_body_size > max_body)
			max_body = _config->locations[i].client_max_body_size;
	}
	if (max_body > 0)
		_parser.set_max_body_size(static_cast<size_t>(max_body));
}

Client::~Client()
{
	if (_cgi != NULL)
	{
		delete _cgi;
		_cgi = NULL;
	}
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
	// Otherwise: still parsing, wait for more bytes
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
	const HttpRequest& req = _parser.request();
	RouteDecision d = _router.route(req, *_config);

	if (d.effective_body_limit >= 0 && req.body.size() > static_cast<size_t>(d.effective_body_limit))
	{
		build_error_response(413);
		return;
	}

	if (d.kind == RouteDecision::KIND_CGI)
	{
		try
		{
			_cgi = new CgiSession(req, d.fs_path, d.cgi_interpreter, *_config);
		}
		catch (const std::exception&)
		{
			_response_built = true;
			_out_buffer = _response_builder.build_error(502, *_config);
		}
		return;
	}

	_response_built = true;
	_out_buffer = _response_builder.build(req, d, *_config);
}

void	Client::build_error_response(int code)
{
	_response_built = true;
	_out_buffer = _response_builder.build_error(code, *_config);
}

CgiSession*	Client::cgi() const { return _cgi; }
bool		Client::has_cgi() const { return _cgi != NULL; }

void	Client::finalize_cgi()
{
	if (_cgi == NULL)
		return;
	touch();
	if (_cgi->was_killed())
	{
		// Killed (timeout, fatal I/O error) -> emit a proper error response
		_out_buffer = _response_builder.build_error(_cgi->failure_code(), *_config);
	}
	else if (_cgi->exited_with_error())
	{
		// Misconfigured interpreter or a script that crashed/exited non-zero:
		// don't ship its (often empty) output as a 200 -> emit a 502
		_out_buffer = _response_builder.build_error(502, *_config);
	}
	else
	{
		_out_buffer = _cgi->build_response();
	}
	delete _cgi;
	_cgi = NULL;
	_response_built = true;
}