/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequestParser.cpp                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/20 13:32:08 by gansari           #+#    #+#             */
/*   Updated: 2026/05/21 12:25:58 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "HttpRequestParser.hpp"
#include <cctype>
#include <cstdlib>
#include <sstream>

// Hard ceiling on a single header line. Some servers (NGINX) use 8KB.
// Anything longer almost certainly means an attack or a bug, never a
// legitimate browser request.
static const size_t	MAX_HEADER_LINE = 8192;

// Default header-section cap when the caller doesn't override.
static const size_t	DEFAULT_MAX_HEADERS_TOTAL = 16384;

// Default body cap when not set by config.
static const size_t	DEFAULT_MAX_BODY = 1048576;  // 1 MiB

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

void	HttpRequestParser::set_max_header_size(size_t bytes)
{
	_max_header_size = bytes;
}

void	HttpRequestParser::set_max_body_size(size_t bytes)
{
	_max_body_size = bytes;
}

HttpRequestParser::State	HttpRequestParser::state() const { return _state; }
int									HttpRequestParser::status_code() const { return _status_code; }
const HttpRequest&					HttpRequestParser::request() const { return _req; }
HttpRequest&						HttpRequestParser::request() { return _req; }

void	HttpRequestParser::fail(int code)
{
	_state = STATE_ERROR;
	_status_code = code;
}

// Pop a CRLF-terminated line from _buf into `out` (without the CRLF).
// Returns true if a complete line was available. We accept bare LF as
// well, even though strict HTTP requires CRLF — many tools (telnet,
// hand-typed clients) use just LF. We do NOT modify _req.method etc here;
// caller is responsible for that.
bool	HttpRequestParser::extract_line(std::string& out)
{
	size_t lf = _buf.find('\n');
	if (lf == std::string::npos)
		return false;

	size_t end = lf;
	if (end > 0 && _buf[end - 1] == '\r')
		--end;  // strip the CR

	out.assign(_buf, 0, end);
	_buf.erase(0, lf + 1);
	return true;
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
}

// ===========================================================
// State handlers
// ===========================================================

// Parse "METHOD SP URI SP HTTP/VERSION CRLF"
// RFC 7230 §3.1.1: exactly two spaces separating three tokens.
bool	HttpRequestParser::parse_request_line()
{
	std::string line;
	if (!extract_line(line))
	{
		// Cap how long we'll wait for a request line. Slowloris-style
		// attacks could send one byte every few seconds; the line cap
		// plus the Client's idle-timeout together prevent abuse.
		if (_buf.size() > MAX_HEADER_LINE)
		{
			fail(414);  // 414 URI Too Long is the closest standard code
			return true;
		}
		return false;
	}

	// Empty line before the request:
	// a leading CRLF as a "robustness" gesture. We silently skip one.[Carriage Retrun(\r) + Line Feed(\n)]
	if (line.empty())
		return true;  // re-enter state with what remains in _buf

	// Request Line:
	// method URI[Unifrom Resourse Identifier(path + query)]
	size_t sp1 = line.find(' ');
	if (sp1 == std::string::npos) { fail(400); return true; }
	size_t sp2 = line.find(' ', sp1 + 1);
	if (sp2 == std::string::npos) { fail(400); return true; }

	_req.method  = line.substr(0, sp1);
	_req.uri     = line.substr(sp1 + 1, sp2 - sp1 - 1);
	_req.version = line.substr(sp2 + 1);

	// Validation: method must be one of the three we support.
	// (Returning 405 here is debatable — 501 Not Implemented is the
	// strict choice for unknown methods, while 405 means "known but
	// not allowed on this resource." We'll use 501 for unknown verbs;
	// the router can later return 405 when a known verb hits a
	// location that disallows it.)
	if (_req.method != "GET" && _req.method != "POST"
		&& _req.method != "DELETE")
	{
		fail(501);
		return true;
	}

	if (_req.uri.empty() || _req.uri[0] != '/')
	{
		fail(400);
		return true;
	}

	// Validation: version must be HTTP/1.0 or HTTP/1.1. We accept both;
	// the subject suggests HTTP/1.0 as reference but doesn't forbid 1.1.
	if (_req.version != "HTTP/1.0" && _req.version != "HTTP/1.1")
	{
		fail(505);  // HTTP Version Not Supported
		return true;
	}

	split_uri();
	_state = STATE_HEADERS;
	return true;
}

// Parse header lines until blank line. Each line: "Name: value CRLF".
// We lowercase the name for case-insensitive lookup.
bool	HttpRequestParser::parse_headers()
{
	std::string line;
	while (extract_line(line))
	{
		// Cap header line length.
		if (line.size() > MAX_HEADER_LINE)
		{
			fail(431);  // Request Header Fields Too Large
			return true;
		}

		// Cap total header bytes.
		_headers_size_so_far += line.size() + 2;  // +2 for the CRLF we ate
		if (_headers_size_so_far > _max_header_size)
		{
			fail(431);
			return true;
		}

		// Empty line marks end of headers.
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

		// "Foo : bar" is malformed.
		if (!name.empty() && std::isspace(
				static_cast<unsigned char>(name[name.size() - 1])))
		{
			fail(400);
			return true;
		}
		if (name.empty())
		{
			fail(400);
			return true;
		}

		// Trim leading/trailing whitespace from value.
		size_t v_start = 0;
		while (v_start < value.size() && std::isspace(
				static_cast<unsigned char>(value[v_start])))
			++v_start;
		size_t v_end = value.size();
		while (v_end > v_start && std::isspace(
				static_cast<unsigned char>(value[v_end - 1])))
			--v_end;
		value = value.substr(v_start, v_end - v_start);

		// case insensetive
		for (size_t i = 0; i < name.size(); ++i)
			name[i] = static_cast<char>(std::tolower(
				static_cast<unsigned char>(name[i])));

		// If the same header appears twice, RFC says we may combine
		// with a comma. For now, last value wins — simpler, and the
		// only header we currently care about duplicating is Set-Cookie
		// (response side, not our problem here).
		_req.headers[name] = value;
	}
	return false;  // need more data
}

// Once headers are done, decide whether there's a body and how to read it.
// Three cases:
//   1. Transfer-Encoding: chunked  → chunked state machine
//   2. Content-Length: N           → read N bytes
//   3. Neither                     → no body, request is complete
void	HttpRequestParser::decide_post_header_state()
{
	std::string te = _req.header("transfer-encoding");
	if (!te.empty())
	{
		// We only support "chunked" as the sole encoding. Anything else
		// (gzip, deflate, etc.) → 501.
		if (te != "chunked")
		{
			fail(501);
			return;
		}
		// RFC 7230: if both TE: chunked and Content-Length are present,
		// the message is suspect; some specs say "use chunked," others
		// "reject." We reject as the safer choice.
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
		// Parse the length. Must be all digits, non-negative, within
		// our configured body cap.
		for (size_t i = 0; i < cl.size(); ++i)
		{
			if (!std::isdigit(static_cast<unsigned char>(cl[i])))
			{
				fail(400);
				return;
			}
		}
		// strtol gives us overflow handling.
		char* endptr = NULL;
		long n = std::strtol(cl.c_str(), &endptr, 10);
		if (endptr == cl.c_str() || *endptr != '\0' || n < 0)
		{
			fail(400);
			return;
		}
		if (static_cast<size_t>(n) > _max_body_size)
		{
			fail(413);  // Payload Too Large
			return;
		}
		_body_remaining = n;
		if (_body_remaining == 0)
		{
			_state = STATE_DONE;
			return;
		}
		_state = STATE_BODY_LENGTH;
		return;
	}

	// No body indicators → request is done.
	_state = STATE_DONE;
}

// Pull up to _body_remaining bytes from _buf into _req.body.
bool	HttpRequestParser::parse_body_length()
{
	if (_buf.empty())
		return false;

	size_t take = _buf.size();
	if (static_cast<long>(take) > _body_remaining)
		take = static_cast<size_t>(_body_remaining);

	// Belt-and-suspenders: this should already be enforced by the
	// Content-Length check at header time, but if the cap was raised
	// mid-stream this stops a runaway append.
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
	return false;  // still need more data
}

// Chunked encoding format:
//   <hex-size>[;ext] CRLF
//   <data of that many bytes> CRLF
//   ...
//   0 CRLF
//   [optional trailer headers]
//   CRLF
//
// We ignore chunk extensions and trailer headers — both are legal but
// rarely used and not required by the subject.
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

	// Strip chunk extensions (everything after ';').
	size_t semi = line.find(';');
	if (semi != std::string::npos)
		line = line.substr(0, semi);

	// Trim trailing whitespace.
	while (!line.empty() && std::isspace(
			static_cast<unsigned char>(line[line.size() - 1])))
		line.resize(line.size() - 1);

	if (line.empty())
	{
		fail(400);
		return true;
	}

	// Parse hex.
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
	if (size == 0)
	{
		_state = STATE_CHUNK_TRAILER;
		return true;
	}
	_state = STATE_CHUNK_DATA;
	return true;
}

// Read _chunk_remaining bytes of data, then expect a CRLF.
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

	// Chunk-data is followed by CRLF before the next size line.
	// We need at least 2 bytes; if we have them, eat them; if they
	// aren't \r\n that's a malformed chunk.
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
		// Ignore trailer line contents.
	}
	return false;  // need more data
}

// ===========================================================
// Main entry: feed bytes and drive the state machine.
// ===========================================================

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
