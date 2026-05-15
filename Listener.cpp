/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Listener.cpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/15 16:28:49 by gansari           #+#    #+#             */
/*   Updated: 2026/05/15 16:28:50 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Listener.hpp"
#include "SocketUtils.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <cerrno>

Listener::Listener(const ServerConfig& config)
	: _fd(-1), _config(&config)
{
	// SOMAXCONN-ish backlog. 128 is the historical kernel cap; modern
	// Linux silently allows more but 128 is plenty for a project server.
	_fd = SocketUtils::make_listener(config.host, config.port, 128);
}

Listener::~Listener()
{
	SocketUtils::safe_close(_fd);
}

int	Listener::fd() const
{
	return _fd;
}

const ServerConfig&	Listener::config() const
{
	return *_config;
}

int	Listener::accept_one()
{
	// We don't need the peer's address for the basic flow, but accept()
	// requires non-null pointers on some old kernels. Passing NULL works
	// on Linux but is non-portable, so we use a real struct.
	struct sockaddr_in peer;
	socklen_t peer_len = sizeof(peer);

	int client_fd = accept(_fd,
		reinterpret_cast<struct sockaddr*>(&peer),
		&peer_len);

	if (client_fd == -1)
	{
		// Subject rule: never check errno after read/write. accept()
		// is technically a "read-like" operation on the listening
		// socket. To stay strictly compliant we just return -1 and
		// let the caller move on. poll() will tell us again if there's
		// still something waiting.
		return -1;
	}

	// New client socket must also be non-blocking. accept() does NOT
	// inherit the parent's O_NONBLOCK on Linux pre-2.6.27 / macOS, so
	// we set it explicitly every time.
	try
	{
		SocketUtils::set_nonblocking_cloexec(client_fd);
	}
	catch (...)
	{
		SocketUtils::safe_close(client_fd);
		return -1;
	}

	return client_fd;
}
