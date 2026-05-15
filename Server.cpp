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
	  _configs(configs),  // own a copy so caller's vector can go out of scope
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
	// One Listener per server block. The validator already rejected
	// duplicate host:port pairs, so each bind is unique.
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
	// Rebuild from scratch each iteration. We could maintain the array
	// incrementally (add on accept, remove on disconnect), but rebuilding
	// is simpler, predictable, and the cost is O(n) on a few hundred fds.
	_pfds.clear();

	// Listeners: only care about POLLIN (new connection waiting).
	for (size_t i = 0; i < _listeners.size(); ++i)
	{
		struct pollfd pfd;
		pfd.fd = _listeners[i]->fd();
		pfd.events = POLLIN;
		pfd.revents = 0;
		_pfds.push_back(pfd);
	}

	// Clients: always want POLLIN (more request data) and only ask
	// for POLLOUT when we actually have something to send. Asking for
	// POLLOUT unconditionally would cause poll() to return immediately
	// every iteration — busy-loop disaster.
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
	// Subject says one poll() drives everything, but doesn't forbid
	// accepting multiple clients per poll wake — accept() is independent
	// of read/write rules. Still, we accept one at a time to keep the
	// loop fair: each iteration of run() can pick up the next client.
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
		return;  // dropped during this poll batch; nothing to do
	Client* c = it->second;

	// POLLHUP/POLLERR/POLLNVAL mean the connection is gone or broken.
	// We could still try to drain remaining data, but for simplicity
	// we drop on any of these. The "no errno after read/write" rule
	// makes finer-grained handling impossible anyway.
	if (revents & (POLLHUP | POLLERR | POLLNVAL))
	{
		drop_client(fd);
		return;
	}

	// Order matters: handle reads before writes within one poll cycle.
	// A read might produce data we then want to write; deferring the
	// write to the NEXT poll iteration would add a needless round-trip.
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

	// Two-pass: collect dead fds, then drop. Mutating _clients while
	// iterating it would invalidate the iterator.
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
	delete it->second;  // destructor closes the fd
	_clients.erase(it);
}

void	Server::run()
{
	std::cout << "entering poll loop (Ctrl-C to stop)\n";

	while (!_stop_requested)
	{
		build_pollfds();

		int n = poll(&_pfds[0], _pfds.size(), POLL_TIMEOUT_MS);

		// poll() returning -1: per subject, we can't check errno.
		// But poll's "no errno" rule is fuzzy — the subject text is
		// about read/write specifically. To be strictly compliant we
		// treat -1 as "loop again"; if it's persistent EBADF or similar
		// the next iteration will hit the same condition and at worst
		// we busy-loop briefly. In practice poll only fails on
		// programmer error.
		if (n < 0)
		{
			// EINTR is the one case where re-looping is unambiguously
			// the right move. Without errno, all -1 returns get the
			// same treatment, which is correct.
			continue;
		}

		if (n == 0)
		{
			// Timeout: no events, but we still want to sweep idle clients.
			sweep_timeouts();
			continue;
		}

		// Walk every pollfd. We can't use revents == 0 as a fast skip
		// because POLLIN/OUT/HUP can each be set independently.
		// Snapshot size: handlers may add/remove from _clients (via
		// drop_client) but never from _listeners, so _pfds size is
		// stable for this iteration's loop.
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
