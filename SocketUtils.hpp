/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SocketUtils.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/15 16:28:16 by gansari           #+#    #+#             */
/*   Updated: 2026/06/24 15:45:06 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SOCKETUTILS_HPP
# define SOCKETUTILS_HPP

# include <string>
# include <stdexcept>

namespace SocketUtils
{
	int		make_listener(const std::string& host, int port, int backlog);
	void	set_nonblocking_cloexec(int fd);
	void	safe_close(int fd);
}

#endif
