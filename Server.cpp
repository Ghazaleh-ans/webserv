/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/15 16:29:29 by gansari           #+#    #+#             */
/*   Updated: 2026/06/01 14:32:36 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include "SocketUtils.hpp"

#include <iostream>
#include <ctime>
#include <cerrno>
#include <cstring>
#include <sstream>

volatile bool	Server::_stop_requested = false;

Server::Server(const std::vector<ServerConfig>& configs)
	: _listeners(),
	  _clients(),
	  _configs(configs),
	  _pfds()
{
}

Server::~Server()
{
	// Clean up every owned pointer. The Listener destructor closes its
	// fd, the Client destructor closes its fd. We delete in reverse
	// order of creation just out of habit.
	for (std::map<int, Client*>::iterator it = _clients.begin();
		it != _clients.end(); ++it)
		delete it->second;
	for (size_t i = 0; i < _listeners.size(); ++i)
		delete _listeners[i];
}

void	Server::request_stop()
{
	_stop_requested = true;
}

void	Server::start()
{
	// One Listener per server block
	for (size_t i = 0; i < _configs.size(); ++i)
	{
		Listener* lis = new Listener(_configs[i]);  // may throw on bind error
		_listeners.push_back(lis);
		std::cout << "listening on " << _configs[i].host
			<< ":" << _configs[i].port
			<< " (fd " << lis->fd() << ")\n";
	}
}

bool	Server::is_listener_fd(int fd, Listener** out) const
{
	for (size_t i = 0; i < _listeners.size(); ++i)
	{
		if (_listeners[i]->fd() == fd)
		{
			*out = _listeners[i];
			return true;
		}
	}
	return false;
}

void	Server::build_pollfds()
{
	// Rebuild from scratch each iteration
	_pfds.clear();

	// Listeners: only care about POLLIN (new connection waiting)
	for (size_t i = 0; i < _listeners.size(); ++i)
	{
		struct pollfd pfd;
		pfd.fd = _listeners[i]->fd();
		pfd.events = POLLIN;
		pfd.revents = 0;
		_pfds.push_back(pfd);
	}

	// Clients: always want POLLIN -> more request data
	// only ask for POLLOUT -> when we actually have something to send
	// Asking for POLLOUT unconditionally -> poll() returns immediately every iteration -> busy-loop disaster
	for (std::map<int, Client*>::iterator it = _clients.begin();
		it != _clients.end(); ++it)
	{
		struct pollfd pfd;
		pfd.fd = it->first;
		pfd.events = POLLIN;
		if (it->second->has_data_to_send())
			pfd.events |= POLLOUT;
		pfd.revents = 0;
		_pfds.push_back(pfd);
	}
}

void	Server::handle_listener_event(Listener* lis)
{
	int client_fd = lis->accept_one();
	if (client_fd < 0)
		return;

	Client* c = new Client(client_fd, lis->config());
	_clients[client_fd] = c;
}

void	Server::handle_client_event(int fd, short revents)
{
	std::map<int, Client*>::iterator it = _clients.find(fd);
	if (it == _clients.end())
		return;  // dropped during this poll batch -> nothing to do
	Client* c = it->second;

	// POLLHUP/POLLERR/POLLNVAL mean the connection is gone or broken
	// POLLIN  = 0000 0001
	// POLLOUT = 0000 0010
	// POLLHUP = 0000 0100 -> the client hung up (closed the connection) -> Like the other side put down the phone
	// POLLERR = 0000 1000 -> a socket error occurred -> Something went wrong at the network level
	// POLLNVAL= 0001 0000 -> the fd is invalid (not open)
	// POLLHUP | POLLERR | POLLNVAL = 0001 1100
	if (revents & (POLLHUP | POLLERR | POLLNVAL))
	{
		drop_client(fd);
		return;
	}

	// handle reads before writes
	if (revents & POLLIN)
	{
		if (!c->on_readable())
		{
			drop_client(fd);
			return;
		}
	}
	if (revents & POLLOUT)
	{
		if (!c->on_writable())
		{
			drop_client(fd);
			return;
		}
	}

	// Client may have decided "I'm done after this send" — honour that.
	if (c->should_close() && !c->has_data_to_send())
		drop_client(fd);
}

void	Server::sweep_timeouts()
{
	std::time_t now = std::time(NULL);

	// Two-pass: collect dead fds, then drop
	std::vector<int> dead;
	for (std::map<int, Client*>::iterator it = _clients.begin();
		it != _clients.end(); ++it)
	{
		if (now - it->second->last_active() > CLIENT_TIMEOUT_SECONDS)
			dead.push_back(it->first);
	}
	for (size_t i = 0; i < dead.size(); ++i)
		drop_client(dead[i]);
}

void	Server::drop_client(int fd)
{
	std::map<int, Client*>::iterator it = _clients.find(fd);
	if (it == _clients.end())
		return;
	delete it->second; // destructor closes the fd
	_clients.erase(it);
}

void	Server::run()
{
	std::cout << "entering poll loop (Ctrl-C to stop)\n";

	while (!_stop_requested)
	{
		build_pollfds();
		// It takes the list of fds, sleeps until one of them has something to do
		// then wakes up and tells you which ones are ready
		// n -> number of fds that have events
		int n = poll(&_pfds[0], _pfds.size(), POLL_TIMEOUT_MS);

		if (n < 0)
		{
			// can't check errno(EINTR) -> Error INTerrupt
			// if SIGINT -> _stop_requested = true -> loop stops
			continue;
		}

		if (n == 0)
		{
			// Timeout: no events, but we still want to sweep idle clients
			sweep_timeouts();
			continue;
		}

		for (size_t i = 0; i < _pfds.size(); ++i)
		{
			if (_pfds[i].revents == 0)
				continue;

			Listener* lis = NULL;
			if (is_listener_fd(_pfds[i].fd, &lis))
			{
				if (_pfds[i].revents & POLLIN)
					handle_listener_event(lis);
			}
			else
			{
				handle_client_event(_pfds[i].fd, _pfds[i].revents);
			}
		}

		sweep_timeouts();
	}

	std::cout << "stopping; closing " << _clients.size()
		<< " active client(s)\n";
}
