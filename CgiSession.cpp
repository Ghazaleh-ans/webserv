/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CgiSession.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/08 12:47:45 by gansari           #+#    #+#             */
/*   Updated: 2026/06/22 15:10:56 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "CgiSession.hpp"
#include "SocketUtils.hpp"

#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <stdexcept>
#include <vector>

static const size_t	CGI_READ_CHUNK = 8192;

// ============================================================
// build_env: CGI/1.1 environment variables
// ============================================================
//
// These are the meta-variables every CGI script expects to find. We
// pass them as KEY=VALUE strings
// the exec setup later converts to char* arrays
//
// We also forward request headers as HTTP_* variables,
// which is how the CGI sees things like User-Agent and Cookie
std::vector<std::string>	CgiSession::build_env(
		const HttpRequest& req,
		const std::string& script_path,
		const ServerConfig& server) const
{
	std::vector<std::string> env;
	std::stringstream ss;

	env.push_back("GATEWAY_INTERFACE=CGI/1.1");
	env.push_back("SERVER_PROTOCOL=HTTP/1.1");
	env.push_back("SERVER_SOFTWARE=webserv/1.0");
	env.push_back("REQUEST_METHOD=" + req.method);
	env.push_back("QUERY_STRING=" + req.query);
	env.push_back("SCRIPT_FILENAME=" + script_path);

	env.push_back("SCRIPT_NAME=" + req.path);

	// PATH_INFO: anything in the URI after the script name. We don't
	// distinguish for now -> so it's empty.
	env.push_back("PATH_INFO=");

	// Server identification
	env.push_back("SERVER_NAME=" + server.host);
	ss.str("");
	ss << server.port;
	env.push_back("SERVER_PORT=" + ss.str());

	// REMOTE_ADDR / REMOTE_HOST -> we don't track the peer address in the current Client
	env.push_back("REMOTE_ADDR=");
	env.push_back("REMOTE_HOST=");

	// Content meta-variables for POST
	if (!req.body.empty())
	{
		ss.str("");
		ss << req.body.size();
		env.push_back("CONTENT_LENGTH=" + ss.str());
	}
	else
	{
		env.push_back("CONTENT_LENGTH=0");
	}
	std::string ct = req.header("content-type");
	if (!ct.empty())
		env.push_back("CONTENT_TYPE=" + ct);

	// REDIRECT_STATUS=200 is a php-CGI quirk: without it, php-cgi refuses
	// to run "for security." Setting it satisfies that check.
	env.push_back("REDIRECT_STATUS=200");

	// --- HTTP_* — every request header forwarded ---
	// "User-Agent: foo" -> HTTP_USER_AGENT=foo (uppercased, dashes to underscores)
	for (std::map<std::string, std::string, CaseInsensitiveLess>::const_iterator
			it = req.headers.begin(); it != req.headers.end(); ++it)
	{
		if (it->first == "content-length" || it->first == "content-type")
			continue;
		std::string name = "HTTP_";
		for (size_t i = 0; i < it->first.size(); ++i)
		{
			char c = it->first[i];
			if (c == '-')
				c = '_';
			else if (c >= 'a' && c <= 'z')
				c = static_cast<char>(c - 'a' + 'A');
			name += c;
		}
		env.push_back(name + "=" + it->second);
	}
	return env;
}

// ============================================================
// Constructor: fork + exec + pipes
// ============================================================
// Two pipes:
//   stdin_pipe[0]  = read-end  -> goes to child's stdin
//   stdin_pipe[1]  = write-end -> we keep, used to feed body
//   stdout_pipe[0] = read-end  -> we keep, used to read response
//   stdout_pipe[1] = write-end -> goes to child's stdout
//
// Child setup steps (after fork, before exec):
//   1. dup2 stdin_pipe[0]  onto fd 0
//   2. dup2 stdout_pipe[1] onto fd 1
//   3. close all other inherited fds (the parent's pipes, listening
//      sockets -> but FD_CLOEXEC handles the latter automatically)
//   4. chdir to the script's directory
//   5. execve the interpreter with the script as argv[1]
//
// Parent setup after fork:
//   - Close the ends we don't need (stdin_pipe[0], stdout_pipe[1])
//   - Set our retained ends non-blocking (poll() rule)
//   - Set retained ends FD_CLOEXEC (so future CGIs don't inherit them)
CgiSession::CgiSession(const HttpRequest& req,
					const std::string& script_path,
					const std::string& interpreter,
					const ServerConfig& server)
	: _pid(-1),
	  _stdin_fd(-1),
	  _stdout_fd(-1),
	  _body_to_write(req.body),
	  _body_write_pos(0),
	  _stdout_buf(),
	  _stdin_closed(false),
	  _child_exited(false),
	  _child_status(0),
	  _killed_by_timeout(false),
	  _last_active(std::time(NULL))
{
	int	stdin_pipe[2]; // server -> CGI child (send request body)
	int	stdout_pipe[2]; // CGI child -> server (receive CGI's response)

	if (pipe(stdin_pipe) == -1)
		throw std::runtime_error("CGI: pipe(stdin) failed");
	if (pipe(stdout_pipe) == -1)
	{
		SocketUtils::safe_close(stdin_pipe[0]);
		SocketUtils::safe_close(stdin_pipe[1]);
		throw std::runtime_error("CGI: pipe(stdout) failed");
	}

	std::vector<std::string> env_strings =
		build_env(req, script_path, server);

	std::string script_dir;
	std::string script_name;
	size_t last_slash = script_path.find_last_of('/');
	if (last_slash != std::string::npos)
	{
		script_dir  = script_path.substr(0, last_slash);
		script_name = script_path.substr(last_slash + 1);
	}
	else
	{
		script_dir  = ".";
		script_name = script_path;
	}

	pid_t pid = fork();
	if (pid == -1)
	{
		SocketUtils::safe_close(stdin_pipe[0]);
		SocketUtils::safe_close(stdin_pipe[1]);
		SocketUtils::safe_close(stdout_pipe[0]);
		SocketUtils::safe_close(stdout_pipe[1]);
		throw std::runtime_error("CGI: fork() failed");
	}

	if (pid == 0)
	{
		// --- CHILD ---
		// From here on, ANY error -> _exit(1)
		// exit() is dangerous in a child process because it:
		// 1. Flushes stdio buffers (parent's buffered data gets written twice)
		// 2. Runs atexit() handlers (corrupts shared parent resources)
		// 3. Runs C++ destructors (destroys parent-owned objects)
		// _exit() skips all three and terminates straight to the kernel

		// Wire stdin/stdout
		if (dup2(stdin_pipe[0], STDIN_FILENO) == -1)  _exit(1);
		if (dup2(stdout_pipe[1], STDOUT_FILENO) == -1) _exit(1);

		close(stdin_pipe[0]);
		close(stdin_pipe[1]);
		close(stdout_pipe[0]);
		close(stdout_pipe[1]);

		// chdir to script directory
		if (chdir(script_dir.c_str()) == -1) {} // non-fatal: execve uses absolute path

		std::vector<char*> argv_vec;
		if (interpreter.empty())
		{
			argv_vec.push_back(const_cast<char*>(script_name.c_str()));
		}
		else
		{
			argv_vec.push_back(const_cast<char*>(interpreter.c_str()));
			argv_vec.push_back(const_cast<char*>(script_name.c_str()));
		}
		argv_vec.push_back(NULL);

		// Build envp from env_strings -> C style
		std::vector<char*> envp;
		for (size_t i = 0; i < env_strings.size(); ++i)
			envp.push_back(const_cast<char*>(env_strings[i].c_str()));
		envp.push_back(NULL);

		// Execute. If execve returns, it failed.
		const char* program = interpreter.empty()
			? script_name.c_str() : interpreter.c_str();
		execve(program, &argv_vec[0], &envp[0]);

		// If we get here, execve failed
		_exit(127);  // command not found code
	}

	// --- PARENT ---
	_pid = pid;

	close(stdin_pipe[0]);
	close(stdout_pipe[1]);

	_stdin_fd = stdin_pipe[1]; // Server writes HTTP request body into this
	_stdout_fd = stdout_pipe[0]; // Server reads the CGI's response from this

	// Non-blocking + FD_CLOEXEC: these pipe fds join the poll() loop.
	try
	{
		SocketUtils::set_nonblocking_cloexec(_stdin_fd);
		SocketUtils::set_nonblocking_cloexec(_stdout_fd);
	}
	catch (...)
	{
		// Couldn't set non-blocking -> kill child
		kill(_pid, SIGKILL);
		waitpid(_pid, NULL, 0);
		SocketUtils::safe_close(_stdin_fd);
		SocketUtils::safe_close(_stdout_fd);
		_stdin_fd = -1;
		_stdout_fd = -1;
		throw;
	}

	// If there's no body to feed, close stdin immediately so the CGI
	// gets EOF and can start producing output
	if (_body_to_write.empty())
	{
		close(_stdin_fd);
		_stdin_fd = -1;
		_stdin_closed = true;
	}
}

CgiSession::~CgiSession()
{
	if (_pid > 0 && !_child_exited)
	{
		kill(_pid, SIGKILL);
		waitpid(_pid, NULL, 0);
	}
	SocketUtils::safe_close(_stdin_fd);
	SocketUtils::safe_close(_stdout_fd);
}

// ============================================================
// Accessors and state queries
// ============================================================

int		CgiSession::stdin_fd() const  { return _stdin_fd; }
int		CgiSession::stdout_fd() const { return _stdout_fd; }
pid_t	CgiSession::pid() const       { return _pid; }
std::time_t	CgiSession::last_active() const { return _last_active; }
void	CgiSession::touch() { _last_active = std::time(NULL); }

bool	CgiSession::wants_write() const
{
	return !_stdin_closed && _body_write_pos < _body_to_write.size();
}

bool	CgiSession::wants_read() const
{
	return _stdout_fd >= 0;
}

bool	CgiSession::is_finished() const
{
	// Normal completion: child exited AND stdout fully drained
	if (_child_exited && _stdout_fd < 0)
		return true;
	// Timeout/kill case: kill_child() already closed both fds
	if (_killed_by_timeout && _stdout_fd < 0)
		return true;
	return false;
}

int		CgiSession::failure_code() const
{
	if (_killed_by_timeout) return 504;
	return 502;
}

bool	CgiSession::was_killed() const
{
	return _killed_by_timeout;
}

// ============================================================
// on_writable_stdin: feed request body to CGI
// ============================================================
bool	CgiSession::on_writable_stdin()
{
	if (_stdin_closed)
		return true;

	size_t remaining = _body_to_write.size() - _body_write_pos;
	if (remaining == 0)
	{
		// Done feeding -> close parent's write end of stdin pipe -> child's read() returns 0 (EOF)
		close(_stdin_fd);
		_stdin_fd = -1;
		_stdin_closed = true;
		return true;
	}

	ssize_t n = write(_stdin_fd, _body_to_write.data() + _body_write_pos, remaining);

	if (n <= 0)
	{
		// Child likely closed stdin (refused our body) -> CGI doesn't need the body or it has already taken what it needs
		close(_stdin_fd);
		_stdin_fd = -1;
		_stdin_closed = true;
		return true;
	}

	_body_write_pos += static_cast<size_t>(n);
	touch();

	if (_body_write_pos >= _body_to_write.size())
	{
		// All written -> Close stdin to signal EOF
		close(_stdin_fd);
		_stdin_fd = -1;
		_stdin_closed = true;
	}
	return true;
}

// ============================================================
// on_readable_stdout: read CGI output
// ============================================================
bool	CgiSession::on_readable_stdout()
{
	char buf[CGI_READ_CHUNK];
	ssize_t n = read(_stdout_fd, buf, sizeof(buf));

	if (n == 0)
	{
		// EOF: CGI closed its stdout
		close(_stdout_fd);
		_stdout_fd = -1;
		return true;
	}
	if (n < 0)
	{
		close(_stdout_fd);
		_stdout_fd = -1;
		return false;
	}

	_stdout_buf.append(buf, static_cast<size_t>(n));
	touch();
	return true;
}

// ============================================================
// check_child: non-blocking waitpid
// ============================================================
void	CgiSession::check_child()
{
	if (_child_exited || _pid <= 0)
		return;

	int status = 0;
	pid_t r = waitpid(_pid, &status, WNOHANG); //WNOHANG: wait but don't hang(block) -> non-blocking
	// r == _pid: child has exited
	if (r == _pid)
	{
		_child_exited = true;
		_child_status = status;
	}
	// r == 0: child still running, try again next iteration
	// r == -1 error
	else if (r == -1)
	{
		_child_exited = true;
		_child_status = -1;
	}
}

void	CgiSession::kill_child()
{
	if (_pid > 0 && !_child_exited)
	{
		_killed_by_timeout = true;
		kill(_pid, SIGKILL);
		// Don't waitpid here -> it would block -> check_child() reaps on the next iteration
	}
	if (_stdin_fd >= 0)
	{
		close(_stdin_fd);
		_stdin_fd = -1;
		_stdin_closed = true;
	}
	if (_stdout_fd >= 0)
	{
		close(_stdout_fd);
		_stdout_fd = -1;
	}
}

// ============================================================
// build_response: parse CGI output, build HTTP response
// ============================================================
// CGI output (_stdout_buf):
//   Status: 404 Not Found\r\n
//   Content-Type: text/html\r\n
//   \r\n
//   <html><body>Not Found</body></html>
//
// HTTP response we emit:
//   HTTP/1.1 404 Not Found\r\n
//   Content-Type: text/html\r\n
//   Content-Length: 36\r\n
//   Connection: close\r\n
//   \r\n
//   <html><body>Not Found</body></html>
//
// "Status:" is stripped and becomes the HTTP status line.
// Content-Type defaults to text/html if the CGI didn't emit one.
std::string	CgiSession::build_response() const
{
	// Find the end of headers -> first blank line (\r\n\r\n or \n\n).
	size_t headers_end = _stdout_buf.find("\r\n\r\n"); // Windows
	size_t sep_len = 4;
	if (headers_end == std::string::npos)
	{
		headers_end = _stdout_buf.find("\n\n"); // Unix
		sep_len = 2;
	}

	std::string status_line = "HTTP/1.1 200 OK";
	std::string extra_headers;
	bool		saw_content_type = false;
	std::string body;

	if (headers_end == std::string::npos)
	{
		// No header/body separation: treat the whole thing as body
		body = _stdout_buf;
	}
	else
	{
		std::string headers_block = _stdout_buf.substr(0, headers_end);
		body = _stdout_buf.substr(headers_end + sep_len);

		size_t pos = 0;
		while (pos < headers_block.size())
		{
			size_t line_end = headers_block.find('\n', pos);
			if (line_end == std::string::npos)
				line_end = headers_block.size();
			size_t content_end = line_end;
			if (content_end > pos && headers_block[content_end - 1] == '\r')
				--content_end;
			std::string line = headers_block.substr(pos, content_end - pos);
			pos = line_end + 1;

			if (line.empty())
				continue;

			size_t colon = line.find(':');
			if (colon == std::string::npos)
				continue;
			std::string name = line.substr(0, colon);
			std::string value = line.substr(colon + 1);
			while (!value.empty() && (value[0] == ' ' || value[0] == '\t'))
				value.erase(0, 1);

			std::string lname = name;
			for (size_t i = 0; i < lname.size(); ++i)
				if (lname[i] >= 'A' && lname[i] <= 'Z')
					lname[i] = static_cast<char>(lname[i] + 32);

			if (lname == "status")
			{
				// "Status: 404 Not Found" -> "HTTP/1.1 404 Not Found"
				status_line = "HTTP/1.1 " + value;
			}
			else if (lname == "content-type")
			{
				saw_content_type = true;
				extra_headers += name + ": " + value + "\r\n";
			}
			else
			{
				// Pass through other headers (Set-Cookie, Location, etc.)
				extra_headers += name + ": " + value + "\r\n";
			}
		}
	}

	if (!saw_content_type)
		extra_headers += "Content-Type: text/html\r\n";

	std::stringstream resp;
	resp << status_line << "\r\n"
		<< extra_headers
		<< "Content-Length: " << body.size() << "\r\n"
		<< "Connection: close\r\n"
		<< "\r\n"
		<< body;
	return resp.str();
}
