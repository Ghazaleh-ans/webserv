/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Client.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/15 16:29:02 by gansari           #+#    #+#             */
/*   Updated: 2026/06/26 16:28:15 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CLIENT_HPP
# define CLIENT_HPP

# include <string>
# include <ctime>
# include "config/ServerConfig.hpp"
# include "http/HttpRequestParser.hpp"
# include "http/Router.hpp"
# include "response/ResponseBuilder.hpp"
# include "cgi/CgiSession.hpp"

// One Client per active TCP connection. Holds:
//   - the fd
//   - an inbound buffer (bytes recv'd, not yet processed)
//   - an outbound buffer (bytes to send when poll says POLLOUT)
//   - a timestamp for idle-timeout detection
//   - a back-pointer to the ServerConfig that should handle requests on it
//
// This is a *passive* object: the Server's poll loop drives it. The
// Client itself never calls poll(), never blocks, never makes
// independent decisions about when to read or write.
class Client
{
public:
	Client(int fd, const ServerConfig& config);
	~Client();

	int						fd() const;
	const ServerConfig&		config() const;

	// State queries the Server uses to decide which poll events to ask for.
	bool					has_data_to_send() const;
	bool					should_close() const;

	// Bookkeeping for idle-timeout detection.
	std::time_t				last_active() const;
	void					touch();  // refresh last_active to "now"

	// Called by Server when poll says this fd is readable.
	// Returns false if the connection should be closed (peer hung up,
	// fatal read error). DOES NOT call errno after recv() - the return
	// value (0 == EOF, >0 == bytes, -1 == "would block, try later")
	// is sufficient.
	bool					on_readable();

	// Called by Server when poll says this fd is writable.
	// Drains _out_buffer as far as the kernel will accept.
	// Returns false on fatal error.
	bool					on_writable();

	// Route the parsed request and build the response into _out_buffer,
	// or start a CgiSession when routing resolves to a CGI script.
	void					build_response();

	// Build an error response when the parser fails with a status code.
	void					build_error_response(int status_code);

	// CGI hooks: the Server polls the CGI's pipe fds and the
	// Client owns the CgiSession. When the CGI finishes (is_finished()
	// true), the Server calls finalize_cgi to drain output into _out_buffer
	CgiSession*				cgi() const;
	bool					has_cgi() const;
	void					finalize_cgi();

private:
	int						_fd;
	const ServerConfig*		_config;
	HttpRequestParser		_parser;
	Router					_router;
	ResponseBuilder			_response_builder;
	std::string				_out_buffer;
	std::time_t				_last_active;
	bool					_should_close;
	bool					_response_built;

	// Non-NULL while a CGI is running for this client. Owned here
	CgiSession*				_cgi;

	Client(const Client&);
	Client&	operator=(const Client&);
};

#endif
