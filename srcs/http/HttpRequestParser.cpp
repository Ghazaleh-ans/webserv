/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequestParser.cpp                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/20 13:32:08 by gansari           #+#    #+#             */
/*   Updated: 2026/06/26 13:16:34 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/HttpRequestParser.hpp"
#include <cctype>
#include <cstdlib>
#include <sstream>

// (NGINX) use 8KB -> anything longer almost certainly means an attack or a bug
static const size_t	MAX_HEADER_LINE = 8192;

// Default header-section cap when the caller doesn't override
static const size_t	DEFAULT_MAX_HEADERS_TOTAL = 16384;

// Default body cap when not set by config
static const size_t	DEFAULT_MAX_BODY = 1048576; // 1 MiB

HttpRequestParser::HttpRequestParser()
{
	_max_header_size = DEFAULT_MAX_HEADERS_TOTAL;
	_max_body_size = DEFAULT_MAX_BODY;
	reset();
}

HttpRequestParser::~HttpRequestParser() {}

void	HttpRequestParser::reset()
{
	_req.reset();
	_state = STATE_REQUEST_LINE;
	_status_code = 200;
	_buf.clear();
	_body_remaining = 0;
	_chunk_remaining = 0;
	_headers_size_so_far = 0;
}

void	HttpRequestParser::set_max_body_size(size_t bytes)
{
	_max_body_size = bytes;
}

HttpRequestParser::State	HttpRequestParser::state() const { return _state; }
int							HttpRequestParser::status_code() const { return _status_code; }
const HttpRequest&			HttpRequestParser::request() const { return _req; }
HttpRequest&				HttpRequestParser::request() { return _req; }

void	HttpRequestParser::fail(int code)
{
	_state = STATE_ERROR;
	_status_code = code;
}

bool	HttpRequestParser::extract_line(std::string& out)
{
	size_t lf = _buf.find('\n');
	if (lf == std::string::npos)
		return false;

	size_t end = lf;
	if (end > 0 && _buf[end - 1] == '\r')
		--end;

	out.assign(_buf, 0, end);
	_buf.erase(0, lf + 1);
	return true;
}

static std::string percent_decode(const std::string& s)
{
	std::string out;
	out.reserve(s.size());
	for (size_t i = 0; i < s.size(); ++i)
	{
		if (s[i] == '%' && i + 2 < s.size()
			&& std::isxdigit(static_cast<unsigned char>(s[i + 1]))
			&& std::isxdigit(static_cast<unsigned char>(s[i + 2])))
		{
			char hex[3] = { s[i + 1], s[i + 2], '\0' };
			unsigned char c = static_cast<unsigned char>(std::strtol(hex, NULL, 16));
			// Keep these bytes percent-encoded instead of decoding them:
			//   '/'           - %2F would smuggle a path separator past the traversal check (/file%2F../secret)
			//   0x00 (NUL)    - truncates C strings (/etc/passwd%00.jpg)
			//   control chars - 0x00-0x1F (everything below the first printable
			//                   byte, space 0x20) and 0x7F (DEL). CR/LF (%0D/%0A)
			//                   matter most: headers are separated by CR/LF, so if
			//                   a decoded path is later echoed into a header (e.g.
			//                   Location: on a redirect), an embedded CR/LF ends
			//                   that header and lets the attacker inject their own
			//                   headers or body -> HTTP response splitting.
			if (c == '/' || c < 0x20 || c == 0x7F)
			{
				out += '%';
				out += s[i + 1];
				out += s[i + 2];
			}
			else
				out += static_cast<char>(c);
			i += 2;
		}
		else
			out += s[i];
	}
	return out;
}

// [path ? query]
void	HttpRequestParser::split_uri()
{
	size_t q = _req.uri.find('?');
	if (q == std::string::npos)
	{
		_req.path = _req.uri;
		_req.query.clear();
	}
	else
	{
		_req.path = _req.uri.substr(0, q);
		_req.query = _req.uri.substr(q + 1);
	}
	_req.path = percent_decode(_req.path);
}

// Parse "METHOD SP URI SP HTTP/VERSION CRLF"
// Line:
// method URI[Unifrom Resourse Identifier(path + query)] HTTP/VERSION \r\n
bool	HttpRequestParser::parse_request_line()
{
	std::string line;
	if (!extract_line(line))
	{
		if (_buf.size() > MAX_HEADER_LINE)
		{
			fail(414); // 414 URI Too Long is the closest standard code
			return true;
		}
		return false;
	}

	// Empty line before the request: [\r\n]
	if (line.empty())
		return true; // re-enter state with what remains in _buf

	size_t sp1 = line.find(' ');
	if (sp1 == std::string::npos) { fail(400); return true; }
	size_t sp2 = line.find(' ', sp1 + 1);
	if (sp2 == std::string::npos) { fail(400); return true; }

	_req.method = line.substr(0, sp1);
	_req.uri = line.substr(sp1 + 1, sp2 - sp1 - 1);
	_req.version = line.substr(sp2 + 1);

	if (_req.method != "GET" && _req.method != "POST" && _req.method != "DELETE")
	{
		fail(501);
		return true;
	}

	if (_req.uri.empty() || _req.uri[0] != '/')
	{
		fail(400);
		return true;
	}

	if (_req.version != "HTTP/1.0" && _req.version != "HTTP/1.1")
	{
		fail(505);
		return true;
	}

	split_uri();
	_state = STATE_HEADERS;
	return true;
}

// Parse header lines until blank line, each line: "Name: value CRLF"
// lowercase the name -> case-insensitive
bool	HttpRequestParser::parse_headers()
{
	std::string line;
	while (extract_line(line))
	{
		if (line.size() > MAX_HEADER_LINE)
		{
			fail(431); // Request Header Fields Too Large
			return true;
		}

		_headers_size_so_far += line.size() + 2; // +2 for the CRLF we ate
		if (_headers_size_so_far > _max_header_size)
		{
			fail(431);
			return true;
		}

		// Empty line marks end of headers
		if (line.empty())
		{
			decide_post_header_state();
			return true;
		}

		// name: value [no space before the :]
		size_t colon = line.find(':');
		if (colon == std::string::npos)
		{
			fail(400);
			return true;
		}

		std::string name = line.substr(0, colon);
		std::string value = line.substr(colon + 1);

		if (!name.empty() && std::isspace(static_cast<unsigned char>(name[name.size() - 1])))
		{
			fail(400);
			return true;
		}
		if (name.empty())
		{
			fail(400);
			return true;
		}

		// Trim leading/trailing whitespace from value
		size_t v_start = 0;
		while (v_start < value.size() && std::isspace(static_cast<unsigned char>(value[v_start])))
			++v_start;
		size_t v_end = value.size();
		while (v_end > v_start && std::isspace(static_cast<unsigned char>(value[v_end - 1])))
			--v_end;
		value = value.substr(v_start, v_end - v_start);

		// case insensetive
		for (size_t i = 0; i < name.size(); ++i)
			name[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(name[i])));

		_req.headers[name] = value;
	}

	// No complete line yet. _buf holds only the current unterminated header
	// line, so a never-terminating header would grow it forever -> reject before that becomes unbounded memory
	if (_buf.size() > MAX_HEADER_LINE)
	{
		fail(431); // Request Header Fields Too Large
		return true;
	}
	return false; // need more data
}

// Once headers are done, decide whether there's a body and how to read it
// Three cases:
//   1. Transfer-Encoding: chunked -> chunked state machine
//   2. Content-Length: N -> read N bytes
//   3. Neither -> no body, request is complete
void	HttpRequestParser::decide_post_header_state()
{
	if (_req.version == "HTTP/1.1" && !_req.has_header("host"))
	{
		fail(400);
		return;
	}

	std::string te = _req.header("transfer-encoding");
	if (!te.empty())
	{
		if (te != "chunked")
		{
			fail(501);
			return;
		}
		// if both TE: chunked and Content-Length are present -> reject
		if (_req.has_header("content-length"))
		{
			fail(400);
			return;
		}
		_state = STATE_CHUNK_SIZE;
		return;
	}

	std::string cl = _req.header("content-length");
	if (!cl.empty())
	{
		for (size_t i = 0; i < cl.size(); ++i)
		{
			if (!std::isdigit(static_cast<unsigned char>(cl[i])))
			{
				fail(400);
				return;
			}
		}
		char* endptr = NULL;
		long n = std::strtol(cl.c_str(), &endptr, 10);
		if (endptr == cl.c_str() || *endptr != '\0' || n < 0)
		{
			fail(400);
			return;
		}
		if (static_cast<size_t>(n) > _max_body_size)
		{
			fail(413); // Payload Too Large
			return;
		}
		_body_remaining = n;
		if (_body_remaining == 0)
		{
			_state = STATE_DONE; //There is nothing to consume
			return;
		}
		_state = STATE_BODY_LENGTH;
		return;
	}

	// No body indicators -> request is done
	_state = STATE_DONE;
}

// Pull up to _body_remaining bytes from _buf into _req.body
bool	HttpRequestParser::parse_body_length()
{
	if (_buf.empty())
		return false;

	size_t take = _buf.size();
	if (static_cast<long>(take) > _body_remaining)
		take = static_cast<size_t>(_body_remaining); // to consume only the amount of bytes content-lenght mentioned

	if (_req.body.size() + take > _max_body_size)
	{
		fail(413);
		return true;
	}

	_req.body.append(_buf, 0, take);
	_buf.erase(0, take);
	_body_remaining -= static_cast<long>(take);

	if (_body_remaining == 0)
	{
		_state = STATE_DONE;
		return true;
	}
	return false; // still need more data
}

	//	POST /upload HTTP/1.1\r\n
	//	Transfer-Encoding: chunked\r\n
	//	\r\n
	//	a; filename=test.txt\r\n -> chunk size line with extension
	//	0123456789\r\n -> 10 bytes of data
	//	0\r\n -> final chunk
	//	\r\n
bool	HttpRequestParser::parse_chunk_size()
{
	std::string line;
	if (!extract_line(line))
	{
		if (_buf.size() > MAX_HEADER_LINE)
		{
			fail(400);
			return true;
		}
		return false;
	}

	// Strip chunk extensions (everything after ';')
	size_t semi = line.find(';');
	if (semi != std::string::npos)
		line = line.substr(0, semi);

	// Trim trailing whitespace
	while (!line.empty() && std::isspace(static_cast<unsigned char>(line[line.size() - 1])))
		line.resize(line.size() - 1);

	if (line.empty())
	{
		fail(400);
		return true;
	}

	// Parse hex
	char* endptr = NULL;
	long size = std::strtol(line.c_str(), &endptr, 16);
	if (endptr == line.c_str() || *endptr != '\0' || size < 0)
	{
		fail(400);
		return true;
	}

	if (_req.body.size() + static_cast<size_t>(size) > _max_body_size)
	{
		fail(413);
		return true;
	}

	_chunk_remaining = size;
	if (size == 0) //no more chunks
	{
		_state = STATE_CHUNK_TRAILER;
		return true;
	}
	_state = STATE_CHUNK_DATA;
	return true;
}

// Read _chunk_remaining bytes of data, then expect a CRLF
bool	HttpRequestParser::parse_chunk_data()
{
	// First, drain remaining chunk bytes.
	if (_chunk_remaining > 0)
	{
		if (_buf.empty())
			return false;

		size_t take = _buf.size();
		if (static_cast<long>(take) > _chunk_remaining)
			take = static_cast<size_t>(_chunk_remaining);

		_req.body.append(_buf, 0, take);
		_buf.erase(0, take);
		_chunk_remaining -= static_cast<long>(take);

		if (_chunk_remaining > 0)
			return false;  // need more data for this chunk
	}

	// Chunk-data is followed by CRLF before the next size line
	if (_buf.size() < 2)
		return false;
	if (_buf[0] != '\r' || _buf[1] != '\n')
	{
		fail(400);
		return true;
	}
	_buf.erase(0, 2);
	_state = STATE_CHUNK_SIZE;
	return true;
}

// After the "0\r\n" we may have trailer headers. Drain lines until
// a blank line. We don't store them.
bool	HttpRequestParser::parse_chunk_trailer()
{
	std::string line;
	while (extract_line(line))
	{
		if (line.empty())
		{
			_state = STATE_DONE;
			return true;
		}
	}
	return false; // need more data
}

// HTTP arrives in unpredictable chuncks over TCP -> we might get half of a header in 
// one recv(), the rest in the next -> we need a state machine to keep the current stage
HttpRequestParser::State	HttpRequestParser::feed(const char* data, size_t len)
{
	if (_state == STATE_DONE || _state == STATE_ERROR)
		return _state;

	_buf.append(data, len);

	bool progressed = true;
	while (progressed && _state != STATE_DONE && _state != STATE_ERROR)
	{
		switch (_state)
		{
			case STATE_REQUEST_LINE:  progressed = parse_request_line();  break;
			case STATE_HEADERS:       progressed = parse_headers();       break;
			case STATE_BODY_LENGTH:   progressed = parse_body_length();   break;
			case STATE_CHUNK_SIZE:    progressed = parse_chunk_size();    break;
			case STATE_CHUNK_DATA:    progressed = parse_chunk_data();    break;
			case STATE_CHUNK_TRAILER: progressed = parse_chunk_trailer(); break;
			default: progressed = false; break;
		}
	}

	return _state;
}
