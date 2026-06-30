#include "extcfg/exceptions.hpp"

namespace extcfg
{

WebServException::WebServException(const std::string& message) : _message(message) {}
WebServException::~WebServException() throw() {}
const char* WebServException::what() const throw() {
	return _message.c_str();
}

IOException::IOException(const std::string& message) : WebServException(message) {}
IOException::~IOException() throw() {}
const char* IOException::what() const throw() {
	return _message.c_str();
}

ConfigException::ConfigException(const std::string& message) : WebServException(message) {}
ConfigException::~ConfigException() throw() {}
const char* ConfigException::what() const throw() {
	return _message.c_str();
}

HttpException::HttpException(const std::string& message) : WebServException(message) {}
HttpException::~HttpException() throw() {}
const char* HttpException::what() const throw() {
	return _message.c_str();
}

SocketException::SocketException(const std::string& message) : WebServException(message) {}
SocketException::~SocketException() throw() {}
const char* SocketException::what() const throw() {
	return _message.c_str();
}

} // namespace extcfg
