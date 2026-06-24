/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Listener.hpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/15 16:28:45 by gansari           #+#    #+#             */
/*   Updated: 2026/06/24 15:39:10 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef LISTENER_HPP
# define LISTENER_HPP

# include "ServerConfig.hpp"

class Listener
{
public:
	Listener(const ServerConfig& config);
	~Listener();

	int						fd() const;
	const ServerConfig&		config() const;

	int						accept_one();

private:
	int						_fd;
	const ServerConfig*		_config;  // non-owning pointer; Server owns the vector

	// Non-copyable: copying would double-close the fd in the destructor
	Listener(const Listener&);
	Listener&	operator=(const Listener&);
};

#endif
