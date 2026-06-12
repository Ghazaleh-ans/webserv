/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SocketUtils.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/15 16:28:16 by gansari           #+#    #+#             */
/*   Updated: 2026/05/19 13:26:38 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SOCKETUTILS_HPP
# define SOCKETUTILS_HPP

# include <string>
# include <stdexcept>

namespace SocketUtils
{
	// Create a listening TCP socket on host:port, set non-blocking,
	// set SO_REUSEADDR, bind, and listen. Returns the listening fd.
	// Throws std::runtime_error on any failure.
	int		make_listener(const std::string& host, int port, int backlog);

	// Apply O_NONBLOCK + FD_CLOEXEC to an fd via fcntl().
	// Subject restricts fcntl to F_SETFL/O_NONBLOCK/FD_CLOEXEC only.
	void	set_nonblocking_cloexec(int fd);

	// Best-effort close that ignores errors. Used in destructors and
	// cleanup paths where we can't react usefully to a close failure.
	void	safe_close(int fd);
}

#endif
