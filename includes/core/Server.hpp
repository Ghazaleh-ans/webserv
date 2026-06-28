/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/15 16:29:32 by gansari           #+#    #+#             */
/*   Updated: 2026/06/19 12:16:43 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVER_HPP
# define SERVER_HPP

# include <vector>
# include <map>
# include <poll.h>
# include "config/ServerConfig.hpp"
# include "network/Listener.hpp"
# include "core/Client.hpp"

class Server
{
public:
	Server(const std::vector<ServerConfig>& configs);
	~Server();

	void	start();

	void	run();

	static void	request_stop();

private:
	std::vector<Listener*>		_listeners;

	std::map<int, Client*>		_clients;

	// The Client owns the CgiSession the Server just borrows pointers for poll registration
	std::map<int, Client*>		_cgi_fd_to_client;

	std::vector<ServerConfig>	_configs;

	std::vector<struct pollfd>	_pfds;

	static volatile bool		_stop_requested;

	static const int			CLIENT_TIMEOUT_SECONDS = 30;

	// CGI timeout must be shorter than CLIENT_TIMEOUT_SECONDS -> the server can send a 504 response before the client connection is swept
	static const int			CGI_TIMEOUT_SECONDS = 25;

	static const int			POLL_TIMEOUT_MS = 1000;

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
