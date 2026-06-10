/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/15 16:29:32 by gansari           #+#    #+#             */
/*   Updated: 2026/06/10 18:58:35 by gansari          ###   ########.fr       */
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
	// pointers are used because Listener is non-copyable
	// (it owns an fd) and std::vector<Listener> would require copies
	// Pointer vector sidesteps that
	std::vector<Listener*>		_listeners;

	// Owned clients keyed by fd, so when poll reports activity on fd N
	std::map<int, Client*>		_clients;

	// CGI fd -> Client owner. When poll reports activity on a CGI pipe
	// fd, we look up the owning Client and dispatch to its CgiSession.
	// (The Client owns the CgiSession; the Server just borrows pointers
	// for poll registration.)
	std::map<int, Client*>		_cgi_fd_to_client;

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

	// Separate timeout for CGI scripts — they often legitimately take
	// longer than a client idle, so a more generous window.
	static const int			CGI_TIMEOUT_SECONDS = 30;

	// poll timeout in milliseconds
	static const int			POLL_TIMEOUT_MS = 1000;

	// --- Loop helpers ---
	void	build_pollfds();
	void	handle_listener_event(Listener* lis);
	void	handle_client_event(int fd, short revents);
	void	handle_cgi_event(int fd, short revents);
	void	register_cgi_fds(Client* c);
	void	unregister_cgi_fds(Client* c);
	void	check_cgi_progress();
	void	sweep_timeouts();
	void	drop_client(int fd);
	bool	is_listener_fd(int fd, Listener** out) const;
	bool	is_cgi_fd(int fd) const;

	Server(const Server&);
	Server&	operator=(const Server&);
};

#endif
