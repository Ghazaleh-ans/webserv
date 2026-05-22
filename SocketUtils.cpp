/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   SocketUtils.cpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/15 16:28:30 by gansari           #+#    #+#             */
/*   Updated: 2026/05/22 11:42:13 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "SocketUtils.hpp"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <sstream>

namespace SocketUtils
{

static void	throw_errno(const std::string& what)
{
	std::stringstream ss;
	ss << what << ": " << std::strerror(errno);
	throw std::runtime_error(ss.str());
}

void	set_nonblocking_cloexec(int fd)
{
	// F_SETFL -> Set File status FLags
	// F_SETFD -> Set File Descriptor flags
	// O_NONBLOCK -> makes the socket non-blocking
	// FD_CLOEXEC -> Close On EXEC -> automatically closes this fd when exec() is called
	if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
		throw_errno("fcntl(F_SETFL, O_NONBLOCK) failed");

	if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1)
		throw_errno("fcntl(F_SETFD, FD_CLOEXEC) failed");
}

void	safe_close(int fd)
{
	if (fd >= 0)
		close(fd);
}

int	make_listener(const std::string& host, int port, int backlog)
{
	// socket() -> creates a socket (like buying a phone)
	// AF_INET = IPv4, SOCK_STREAM = TCP, protocol 0 = pick default.
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1)
		throw_errno("socket() failed");

	// SO_REUSEADDR lets us restart the server immediately after a crash
	// or normal shutdown without waiting for TIME_WAIT
	int yes = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1)
	{
		safe_close(fd);
		throw_errno("setsockopt(SO_REUSEADDR) failed");
	}

	try
	{
		set_nonblocking_cloexec(fd);
	}
	catch (...)
	{
		safe_close(fd);
		throw;
	}

	// struct sockaddr_in -> IPv4 address -> right format for bind() and connect()
	// sockaddr_in: family, port (network byte order), 4-byte address
	struct sockaddr_in addr;
	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	// htons -> Host To Network Short
	// converts a 2-byte integer from your CPU's byte order to network byte order
	// port 8080 -> 0x1f90 -> high byte = 1F, low byte = 90 
	// Network stores (1F 90) -> forwards
	// CPU stores (90 1F) -> backward
	addr.sin_port = htons(static_cast<uint16_t>(port));

	// inet_pton -> Internet Presentation To Network
	// converts the IP address string to 4-byte binary format
	// "127.0.0.1" -> 0x7F000001
	if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1)
	{
		safe_close(fd);
		throw std::runtime_error("invalid host address: " + host);
	}

	// bind() -> assigns the socket an address and port (like getting a phone number)
	if (bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == -1)
	{
		safe_close(fd);
		std::stringstream ss;
		ss << "bind(" << host << ":" << port << ") failed: "
			<< std::strerror(errno);
		throw std::runtime_error(ss.str());
	}

	// listen() -> starts waiting for incoming calls
	// mark this socket as accepting connections
	// backlog is the kernel queue depth for connections that have completed the
	// TCP handshake but we haven't accept()'d yet
	if (listen(fd, backlog) == -1)
	{
		safe_close(fd);
		throw_errno("listen() failed");
	}

	return fd;
}

}
