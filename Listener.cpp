/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Listener.cpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/15 16:28:49 by gansari           #+#    #+#             */
/*   Updated: 2026/06/23 10:13:38 by gansari          ###   ########.fr       */
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
	// SOMAXCONN -> SOcket MAXimum CONNections -> backlog -> pending connections in queue
	// waiting to be accept
	// For backlog: modern Linux silently allows more but 128 is plenty for a project server
	_fd = SocketUtils::make_listener(config.host, config.port, SOMAXCONN);
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
	struct sockaddr_in peer;
	socklen_t peer_len = sizeof(peer);

	int client_fd = accept(_fd, reinterpret_cast<struct sockaddr*>(&peer), &peer_len);

	if (client_fd == -1)
	{
		// no errno
		return -1;
	}

	// make the new client nonblocking
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
