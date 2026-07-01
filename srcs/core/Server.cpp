/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/15 16:29:29 by gansari           #+#    #+#             */
/*   Updated: 2026/07/01 12:34:52 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "core/Server.hpp"
#include "network/SocketUtils.hpp"

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
	for (size_t i = 0; i < _configs.size(); ++i)
	{
		Listener* lis = new Listener(_configs[i]);
		_listeners.push_back(lis);
		std::cout << "listening on " << _configs[i].host << ":" << _configs[i].port << " (fd " << lis->fd() << ")\n";
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

	for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		struct pollfd pfd;
		pfd.fd = it->first;
		pfd.events = 0;
		if (!it->second->has_cgi()) // don't read new request data while CGI is in progress
			pfd.events |= POLLIN;
		if (it->second->has_data_to_send()) // only watch for writability when there's something to send
			pfd.events |= POLLOUT;
		pfd.revents = 0;
		_pfds.push_back(pfd);
	}

	for (std::map<int, Client*>::iterator it = _cgi_fd_to_client.begin(); it != _cgi_fd_to_client.end(); ++it)
	{
		Client* c = it->second;
		if (c->cgi() == NULL)
			continue;
		struct pollfd pfd;
		pfd.fd = it->first;
		pfd.events = 0;
		pfd.revents = 0;
		if (it->first == c->cgi()->stdin_fd() && c->cgi()->wants_stdin_write())
			pfd.events |= POLLOUT;
		else if (it->first == c->cgi()->stdout_fd() && c->cgi()->wants_stdout_read())
			pfd.events |= POLLIN;
		if (pfd.events != 0)
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
		return;
	Client* c = it->second;

	// POLLHUP/POLLERR/POLLNVAL mean the connection is gone or broken
	if (revents & (POLLHUP | POLLERR | POLLNVAL))
	{
		drop_client(fd);
		return;
	}

	if (revents & POLLIN)
	{
		if (!c->on_readable())
		{
			drop_client(fd);
			return;
		}
		// If on_readable started a CGI session -> register its pipe fd with the poll loop for them next iteration
		if (c->has_cgi())
			register_cgi_fds(c);
	}
	if (revents & POLLOUT)
	{
		if (!c->on_writable())
		{
			drop_client(fd);
			return;
		}
	}

	// Client may have decided "I'm done after this send"
	if (c->should_close() && !c->has_data_to_send())
		drop_client(fd);
}

bool	Server::is_cgi_fd(int fd) const
{
	return _cgi_fd_to_client.find(fd) != _cgi_fd_to_client.end();
}

void	Server::register_cgi_fds(Client* c)
{
	if (c->cgi() == NULL)
		return;
	if (c->cgi()->stdin_fd() >= 0)
		_cgi_fd_to_client[c->cgi()->stdin_fd()] = c;
	if (c->cgi()->stdout_fd() >= 0)
		_cgi_fd_to_client[c->cgi()->stdout_fd()] = c;
}

void	Server::unregister_cgi_fds(Client* c)
{
	for (std::map<int, Client*>::iterator it = _cgi_fd_to_client.begin(); it != _cgi_fd_to_client.end(); )
	{
		if (it->second == c)
		{
			std::map<int, Client*>::iterator to_erase = it;
			++it;
			_cgi_fd_to_client.erase(to_erase);
		}
		else
		{
			++it;
		}
	}
}

void	Server::handle_cgi_event(int fd, short revents)
{
	std::map<int, Client*>::iterator it = _cgi_fd_to_client.find(fd);
	if (it == _cgi_fd_to_client.end())
		return;
	Client* c = it->second;
	if (c->cgi() == NULL)
		return;
	c->touch();
	CgiSession* cgi = c->cgi();
	bool ok = true;
	// POLLIN -> the child wrote some output into the pipe -> bytes are ready to read
	// POLLHUP -> the child closes its stdout and(exited or finished writing) -> pipe is EOF
	if (fd == cgi->stdout_fd() && (revents & (POLLIN | POLLHUP)))
	{
		ok = cgi->on_readable_stdout();
	}
	else if (fd == cgi->stdin_fd())
	{
		// POLLOUT -> the pipe buffer has space
		// POLLHUP | POLLERR | POLLNVAL -> the child closed it's stdin early
		if (revents & (POLLHUP | POLLERR | POLLNVAL))
		{
			// on_writable_stdin() will write, get EPIPE (n<=0), close fd
			cgi->on_writable_stdin();
		}
		else if (revents & POLLOUT)
		{
			ok = cgi->on_writable_stdin();
		}
	}

	if (!ok)
	{
		// Force termination -> finalize_cgi in check_cgi_progress will produce a 502
		cgi->kill_child();
	}
}

// Walk every running CGI ->  check_child() -> finalize the ones that are done
void	Server::check_cgi_progress()
{
	std::time_t now = std::time(NULL);
	std::vector<Client*> to_finalize;

	for (std::map<int, Client*>::iterator it = _clients.begin();
		it != _clients.end(); ++it)
	{
		Client* c = it->second;
		if (!c->has_cgi())
			continue;
		CgiSession* cgi = c->cgi();
		cgi->check_child();

		if (!cgi->is_finished() && now - cgi->last_active() > CGI_TIMEOUT_SECONDS)
		{
			cgi->kill_child();
		}

		if (cgi->is_finished())
			to_finalize.push_back(c);
	}

	for (size_t i = 0; i < to_finalize.size(); ++i)
	{
		Client* c = to_finalize[i];
		unregister_cgi_fds(c);
		c->finalize_cgi();
	}
}

void	Server::sweep_timeouts()
{
	std::time_t now = std::time(NULL);

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
	unregister_cgi_fds(it->second);
	delete it->second;
	_clients.erase(it);
}

void	Server::run()
{
	std::cout << "entering poll loop (Ctrl-C to stop)\n";

	while (!_stop_requested)
	{
		build_pollfds();
		int n = poll(&_pfds[0], _pfds.size(), POLL_TIMEOUT_MS);

		if (n < 0)
		{
			// can't check errno(EINTR) -> Error INTerrupt
			// if SIGINT -> _stop_requested = true -> loop stops
			continue;
		}

		if (n == 0)
		{
			check_cgi_progress();
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
			else if (is_cgi_fd(_pfds[i].fd))
			{
				handle_cgi_event(_pfds[i].fd, _pfds[i].revents);
			}
			else
			{
				handle_client_event(_pfds[i].fd, _pfds[i].revents);
			}
		}

		check_cgi_progress();
		sweep_timeouts();
	}

	std::cout << "stopping; closing " << _clients.size() << " active client(s)\n";
}
