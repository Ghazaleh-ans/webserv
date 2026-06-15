/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CgiSession.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/08 12:47:35 by gansari           #+#    #+#             */
/*   Updated: 2026/06/15 15:02:45 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CGISESSION_HPP
# define CGISESSION_HPP

# include <string>
# include <vector>
# include <sys/types.h>
# include <ctime>

# include "HttpRequest.hpp"
# include "LocationConfig.hpp"
# include "ServerConfig.hpp"

// Lifecycle:
//   Constructor:  fork + exec, create stdin/stdout pipes
//   Polling:      on_readable_stdout (POLLIN on stdout),
//                 on_writable_stdin  (POLLOUT on stdin),
//                 check_child         (waitpid WNOHANG each loop iteration)
//   Termination:  when child has exited AND stdout has been fully drained,
//                 is_finished() returns true and the Client owning this
//                 session calls finalize() to build an HTTP response
//
// The Client that triggered this session owns it (new/delete). The Server
// borrows the pipe fds for poll registration and calls back into the Client
// when the session finishes.
class CgiSession
{
public:
	// Build and start. Throws std::runtime_error on fork/pipe/exec failure;
	// the caller (Client) catches and emits 500
	CgiSession(const HttpRequest& req,
			   const std::string& script_path,
			   const std::string& interpreter,
			   const ServerConfig& server);
	~CgiSession();

	// fd accessors — Server registers these with poll().
	int		stdin_fd() const;
	int		stdout_fd() const;
	pid_t	pid() const;

	// State queries the Server uses to decide which poll events to ask for.
	bool	wants_write() const;   // true when body still has bytes to write
	bool	wants_read() const;    // true while stdout pipe is open
	bool	is_finished() const;   // child exited AND stdout closed AND body fully sent

	// Idle-timeout integration. Server's sweep_timeouts() uses this.
	std::time_t	last_active() const;
	void		touch();

	// Event handlers — return false to signal "abort, kill child."
	bool	on_writable_stdin();
	bool	on_readable_stdout();

	// Called each poll iteration regardless of fd events: non-blocking
	// waitpid to detect child exit. Sets internal flag.
	void	check_child();

	// Kill the child process (used on timeout or fatal error)
	void	kill_child();

	// Build the HTTP response from the CGI's output. Called by the
	// Client once is_finished() returns true.
	std::string	build_response() const;

	// True if we had to SIGKILL the child (timeout / fatal I/O error).
	// Used by Client to decide between build_response (normal) and
	// build_error (failure).
	bool		was_killed() const;

	// Status code to use if the CGI fails altogether (timeout, exec
	// failure, weird exit). 504 for timeout, 502 for everything else.
	int		failure_code() const;

private:
	pid_t		_pid;
	int			_stdin_fd;
	int			_stdout_fd;

	std::string	_body_to_write;   // request body queued for stdin
	size_t		_body_write_pos;  // how many bytes already written
	std::string	_stdout_buf;      // bytes read from CGI stdout
	bool		_stdin_closed;
	bool		_child_exited;
	int			_child_status;
	bool		_killed_by_timeout;

	std::time_t	_last_active;

	// Setup helpers used by the constructor.
	std::vector<std::string>	build_env(const HttpRequest& req,
										  const std::string& script_path,
										  const ServerConfig& server) const;

	CgiSession(const CgiSession&);
	CgiSession&	operator=(const CgiSession&);
};

#endif
