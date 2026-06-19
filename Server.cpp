/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/15 16:29:29 by gansari           #+#    #+#             */
/*   Updated: 2026/06/18 13:29:42 by gansari          ###   ########.fr       */
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
	// Reject configs that share the same host:port  
	// Only one socket can bind to an address
	for (size_t i = 0; i < _configs.size(); ++i)
	{
		for (size_t j = i + 1; j < _configs.size(); ++j)
		{
			if (_configs[i].host == _configs[j].host &&
				_configs[i].port == _configs[j].port)
			{
				std::ostringstream msg;
				msg << "duplicate server: " << _configs[i].host
					<< ":" << _configs[i].port
					<< " is defined more than once in the config file";
				throw std::runtime_error(msg.str());
			}
		}
	}

	for (size_t i = 0; i < _configs.size(); ++i)
	{
		Listener* lis = new Listener(_configs[i]);
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
	// when a CGI is running for this client, we DON'T poll
	// the client socket for reads — we're not going to act on more
	// request data until the CGI completes. We do still poll for writes
	// once the CGI is done and _out_buffer has bytes.
	for (std::map<int, Client*>::iterator it = _clients.begin();
		it != _clients.end(); ++it)
	{
		struct pollfd pfd;
		pfd.fd = it->first;
		pfd.events = 0;
		if (!it->second->has_cgi())
			pfd.events |= POLLIN;
		if (it->second->has_data_to_send())
			pfd.events |= POLLOUT;
		pfd.revents = 0;
		_pfds.push_back(pfd);
	}

	// CGI pipe fds. Each running CGI may have a stdin fd (we write to
	// it) and a stdout fd (we read from it). We ask for the relevant
	// event on each.
	for (std::map<int, Client*>::iterator it = _cgi_fd_to_client.begin();
		it != _cgi_fd_to_client.end(); ++it)
	{
		Client* c = it->second;
		if (c->cgi() == NULL)
			continue;
		struct pollfd pfd;
		pfd.fd = it->first;
		pfd.events = 0;
		pfd.revents = 0;
		if (it->first == c->cgi()->stdin_fd() && c->cgi()->wants_write())
			pfd.events |= POLLOUT;
		else if (it->first == c->cgi()->stdout_fd() && c->cgi()->wants_read())
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
		// If on_readable started a CGI session, register its pipe fds
		// with the poll loop so we get events for them next iteration.
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

// ============================================================
// CGI fd management
// ============================================================
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
	// Walk the map and erase any entry pointing to this client.
	// (We can't reference the now-deleted CgiSession's fds.)
	for (std::map<int, Client*>::iterator it = _cgi_fd_to_client.begin();
		it != _cgi_fd_to_client.end(); )
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
	// POLLIN -> the child wrote some output into the pipe, bytes are ready to read
	// POLLHUP -> the child closes its stdout and(exited or finished writing), pipe is EOF
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
		// Force termination ->finalize_cgi in check_cgi_progress will
		// produce a 502.
		cgi->kill_child();
	}
}

// Walk every running CGI, check_child(), and finalize the ones that
// are done. Called once per loop iteration, after the event dispatch.
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

		// Timeout check: if the CGI has been silent too long, kill it.
		// kill_child() also closes our pipe fds so is_finished() flips
		// to true on the same iteration — we finalize below.
		if (!cgi->is_finished()
			&& now - cgi->last_active() > CGI_TIMEOUT_SECONDS)
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
	unregister_cgi_fds(it->second);
	delete it->second; // destructor closes the fd and kills any CGI
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
			// and check whether any CGI children have exited.
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

	std::cout << "stopping; closing " << _clients.size()
		<< " active client(s)\n";
}
