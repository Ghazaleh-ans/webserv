/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/15 16:29:32 by gansari           #+#    #+#             */
/*   Updated: 2026/05/22 10:21:57 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVER_HPP
# define SERVER_HPP

# include <vector>
# include <map>
# include <poll.h>
# include "ServerConfig.hpp"
# include "Listener.hpp"
# include "Client.hpp"

class Server
{
public:
	Server(const std::vector<ServerConfig>& configs);
	~Server();

	// Open every listening socket
	void	start();

	// Run the poll loop until stop() is called
	void	run();

	// Causes run() to return cleanly. Safe to call from a signal
	// handler because it only sets a sig_atomic_t-like bool.
	static void	request_stop();

private:
	// Owned listeners. We use pointers because Listener is non-copyable
	// (it owns an fd) and std::vector<Listener> would require copies
	// in C++98 (no move semantics). Pointer vector sidesteps that.
	std::vector<Listener*>		_listeners;

	// Owned clients keyed by fd, so when poll reports activity on fd N
	// we can O(log n) find the right Client.
	std::map<int, Client*>		_clients;

	// The original configs, kept alive for the lifetime of the server
	// because Listeners hold references into this vector.
	std::vector<ServerConfig>	_configs;

	// The pollfd array, rebuilt every iteration. Keeping it as a
	// member instead of a local avoids reallocating each loop.
	std::vector<struct pollfd>	_pfds;

	// Set by request_stop() to break run()'s loop.
	static volatile bool		_stop_requested;

	// Idle-timeout in seconds. Subject requires "request should never
	// hang indefinitely" — we enforce it by reaping silent clients.
	static const int			CLIENT_TIMEOUT_SECONDS = 30;

	// poll timeout in milliseconds. Doesn't need to be short — we
	// only need to wake up periodically for the idle-timeout sweep
	// and the stop check.
	static const int			POLL_TIMEOUT_MS = 1000;

	// --- Loop helpers ---
	void	build_pollfds();
	void	handle_listener_event(Listener* lis);
	void	handle_client_event(int fd, short revents);
	void	sweep_timeouts();
	void	drop_client(int fd);
	bool	is_listener_fd(int fd, Listener** out) const;

	Server(const Server&);
	Server&	operator=(const Server&);
};

#endif
