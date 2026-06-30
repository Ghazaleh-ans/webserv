#pragma once

#include <string>
#include <exception>

// Imported verbatim from the ../webserv project's config layer, wrapped in the
// `extcfg` namespace so its ServerConfig/LocationConfig don't collide with this
// project's same-named types. All exceptions derive from std::exception, so
// main()'s `catch (const std::exception&)` still catches them.
namespace extcfg
{

class WebServException : public std::exception {
	public:
		WebServException(const std::string& message);
		virtual ~WebServException() throw();
		virtual const char* what() const throw();
	protected:
		std::string _message;
};

class IOException : public WebServException {
	public:
		IOException(const std::string& message);
		virtual ~IOException() throw();
		virtual const char* what() const throw();
};

class ConfigException : public WebServException {
	public:
		ConfigException(const std::string& message);
		virtual ~ConfigException() throw();
		virtual const char* what() const throw();
};

class HttpException : public WebServException {
	public:
		HttpException(const std::string& message);
		virtual ~HttpException() throw();
		virtual const char* what() const throw();
};

class SocketException : public WebServException {
	public:
		SocketException(const std::string& message);
		virtual ~SocketException() throw();
		virtual const char* what() const throw();
};

} // namespace extcfg
