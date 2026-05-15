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

// Throw helper that includes errno's text. We can use strerror(errno)
// here because the subject only forbids checking errno *after read/write*;
// for setup syscalls (socket/bind/listen) it's the standard way to report
// what went wrong.
static void	throw_errno(const std::string& what)
{
	std::stringstream ss;
	ss << what << ": " << std::strerror(errno);
	throw std::runtime_error(ss.str());
}

void	set_nonblocking_cloexec(int fd)
{
	// F_SETFL with O_NONBLOCK makes read()/write()/accept()/recv()/send()
	// return immediately with EAGAIN/EWOULDBLOCK instead of sleeping.
	// This is the core mechanism that lets one poll() drive everything.
	if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
		throw_errno("fcntl(F_SETFL, O_NONBLOCK) failed");

	// FD_CLOEXEC closes this fd automatically when we exec() a CGI.
	// Without it, the CGI process inherits our listening sockets, which
	// would (a) be a security hole and (b) hold the ports open after
	// webserv exits if the CGI lingers.
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
	// AF_INET = IPv4, SOCK_STREAM = TCP, protocol 0 = pick default.
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1)
		throw_errno("socket() failed");

	// SO_REUSEADDR lets us restart the server immediately after a crash
	// or normal shutdown without waiting for TIME_WAIT (~60 seconds).
	// Without it, every restart during development becomes "Address
	// already in use" for a minute. Set BEFORE bind() — too late after.
	int yes = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1)
	{
		safe_close(fd);
		throw_errno("setsockopt(SO_REUSEADDR) failed");
	}

	// Non-blocking applies to the listening socket too: when we call
	// accept() with no pending connections, it returns EAGAIN instead
	// of sleeping. poll() will tell us when there's actually one waiting.
	try { set_nonblocking_cloexec(fd); }
	catch (...) { safe_close(fd); throw; }

	// Fill sockaddr_in by hand. struct sockaddr_in is the IPv4 address
	// shape: family, port (network byte order!), 4-byte address.
	struct sockaddr_in addr;
	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(static_cast<uint16_t>(port)); //Host To Network Short

	// inet_aton is BSD; inet_pton is POSIX and works on every modern
	// system. It parses "127.0.0.1" or "0.0.0.0" into 4 bytes.
	if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1)
	{
		safe_close(fd);
		throw std::runtime_error("invalid host address: " + host);
	}

	if (bind(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == -1)
	{
		safe_close(fd);
		std::stringstream ss;
		ss << "bind(" << host << ":" << port << ") failed: "
			<< std::strerror(errno);
		throw std::runtime_error(ss.str());
	}

	// listen(): mark this socket as accepting connections. backlog is
	// the kernel queue depth for connections that have completed the
	// TCP handshake but we haven't accept()'d yet. SOMAXCONN (128 on
	// Linux) is the common ceiling.
	if (listen(fd, backlog) == -1)
	{
		safe_close(fd);
		throw_errno("listen() failed");
	}

	return fd;
}

}  // namespace SocketUtils
