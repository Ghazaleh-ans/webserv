/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Listener.hpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/15 16:28:45 by gansari           #+#    #+#             */
/*   Updated: 2026/05/19 14:19:18 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef LISTENER_HPP
# define LISTENER_HPP

# include "ServerConfig.hpp"

// One Listener per host:port pair in the config. Holds the listening
// socket fd and a back-pointer to the ServerConfig that produced it,
// so when accept() yields a new client we know which config governs it.
class Listener
{
public:
	Listener(const ServerConfig& config);
	~Listener();

	int						fd() const;
	const ServerConfig&		config() const;

	// Accept one waiting client. Returns the new client fd, or -1 if
	// accept() would block (no client actually pending — should not
	// happen if poll said this fd was readable, but defensive).
	// The returned fd is already set non-blocking.
	int						accept_one();

private:
	int						_fd;
	const ServerConfig*		_config;  // non-owning pointer; Server owns the vector

	// Non-copyable: copying would double-close the fd in the destructor.
	// In C++11 we'd use = delete; in C++98 we just declare them private.
	Listener(const Listener&);
	Listener&	operator=(const Listener&);
};

#endif
