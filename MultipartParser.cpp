/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   MultipartParser.cpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/03 16:57:21 by gansari           #+#    #+#             */
/*   Updated: 2026/06/05 16:19:41 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "MultipartParser.hpp"
#include <cctype>
#include <cstring>

MultipartParser::MultipartParser() : _parts(), _status(0) {}
MultipartParser::~MultipartParser() {}

int		MultipartParser::status_code() const { return _status; }
const std::vector<MultipartPart>&	MultipartParser::parts() const { return _parts; }

// Case-insensitive prefix match. Used to find "Content-Disposition:"
// regardless of case ("content-disposition:", "CONTENT-DISPOSITION:" etc.)
bool	MultipartParser::starts_with_ci(const std::string& s,
									 const std::string& prefix)
{
	if (s.size() < prefix.size())
		return false;
	for (size_t i = 0; i < prefix.size(); ++i)
	{
		char a = static_cast<char>(std::tolower(
			static_cast<unsigned char>(s[i])));
		char b = static_cast<char>(std::tolower(
			static_cast<unsigned char>(prefix[i])));
		if (a != b)
			return false;
	}
	return true;
}

// ============================================================
// extract_boundary: pull boundary= out of a Content-Type header
// ============================================================
// Content-Type values for multipart look like:
//   multipart/form-data; boundary=----WebKitFormBoundaryXYZ
//   multipart/form-data; charset=utf-8; boundary=mygroup
//   multipart/form-data;boundary="quoted boundary"
//
// We do a forgiving parse: find "boundary=" anywhere after the media
// type, strip surrounding quotes if any, return whatever's left up to
// the next ';' or end of string
std::string	MultipartParser::extract_boundary(const std::string& content_type)
{
	// First confirm the type is multipart/form-data
	if (!starts_with_ci(content_type, "multipart/form-data"))
		return "";

	// Find "boundary=" — search case-insensitively by scanning.
	std::string lower = content_type;
	for (size_t i = 0; i < lower.size(); ++i)
		lower[i] = static_cast<char>(std::tolower(
			static_cast<unsigned char>(lower[i])));

	size_t pos = lower.find("boundary=");
	if (pos == std::string::npos)
		return "";
	pos += 9;  // length of "boundary="

	// Extract value: stop at ';' or end of string.
	size_t end = content_type.find(';', pos);
	if (end == std::string::npos)
		end = content_type.size();

	std::string value = content_type.substr(pos, end - pos);

	// Trim whitespace.
	size_t s = 0;
	while (s < value.size() && std::isspace(
			static_cast<unsigned char>(value[s])))
		++s;
	size_t e = value.size();
	while (e > s && std::isspace(
			static_cast<unsigned char>(value[e - 1])))
		--e;
	value = value.substr(s, e - s);

	// Strip surrounding double quotes if present.
	if (value.size() >= 2 && value[0] == '"'
		&& value[value.size() - 1] == '"')
		value = value.substr(1, value.size() - 2);

	return value;
}

// ============================================================
// parse_disposition: extract name=... and filename=... 
// ============================================================
// Input is the VALUE of Content-Disposition (after the colon).
// e.g. 'form-data; name="upload"; filename="report.pdf"'
//
// We scan for `name="..."` and `filename="..."` substrings.
// This is loose but matches every browser we've encountered.
// A fully RFC-correct parser would also handle filename* (RFC 5987)
// for non-ASCII names; we don't.
void	MultipartParser::parse_disposition(const std::string& value,
										  MultipartPart& part)
{
	// Helper lambda would be nice here. C++98 → write inline.
	const char* keys[] = { "name=", "filename=", NULL };
	std::string* targets[] = { &part.name, &part.filename };

	for (size_t k = 0; keys[k] != NULL; ++k)
	{
		size_t pos = value.find(keys[k]);
		if (pos == std::string::npos)
			continue;

		// Boundary check: the match must be either at start or after
		// a non-letter (so "filename=" inside "myfilename=" doesn't
		// match "name=").
		if (pos > 0)
		{
			char before = value[pos - 1];
			if (std::isalpha(static_cast<unsigned char>(before)))
				continue;
		}

		size_t start = pos + std::strlen(keys[k]);
		if (start >= value.size())
			continue;

		std::string extracted;
		if (value[start] == '"')
		{
			// Quoted: read until closing quote.
			size_t end = value.find('"', start + 1);
			if (end == std::string::npos)
				continue;
			extracted = value.substr(start + 1, end - start - 1);
		}
		else
		{
			// Unquoted: read until ';' or whitespace.
			size_t end = start;
			while (end < value.size()
				&& value[end] != ';'
				&& !std::isspace(static_cast<unsigned char>(value[end])))
				++end;
			extracted = value.substr(start, end - start);
		}
		*targets[k] = extracted;
	}
}

// ============================================================
// parse_part_headers: read one part's header block
// ============================================================
// Input: the bytes between the boundary and the blank-line marker,
// e.g. "Content-Disposition: form-data; name=\"foo\"\r\nContent-Type: text/plain"
//
// We process line by line. Header values can be folded (continuation
// lines starting with whitespace) per RFC, but we don't handle that
// — browsers never produce folded headers in multipart bodies.
bool	MultipartParser::parse_part_headers(const std::string& headers,
										   MultipartPart& part)
{
	size_t pos = 0;
	while (pos < headers.size())
	{
		// Find end of line (CRLF, or LF, or end of string).
		size_t line_end = pos;
		while (line_end < headers.size() && headers[line_end] != '\n')
			++line_end;

		// Slice the line, stripping the optional CR.
		size_t content_end = line_end;
		if (content_end > pos && headers[content_end - 1] == '\r')
			--content_end;
		std::string line = headers.substr(pos, content_end - pos);
		pos = line_end + 1;  // step past \n for next iteration

		if (line.empty())
			continue;

		// Split at first colon.
		size_t colon = line.find(':');
		if (colon == std::string::npos)
		{
			_status = 400;
			return false;
		}

		std::string name = line.substr(0, colon);
		std::string value = line.substr(colon + 1);

		// Trim leading/trailing whitespace from value.
		size_t s = 0;
		while (s < value.size() && std::isspace(
				static_cast<unsigned char>(value[s])))
			++s;
		size_t e = value.size();
		while (e > s && std::isspace(
				static_cast<unsigned char>(value[e - 1])))
			--e;
		value = value.substr(s, e - s);

		// Dispatch by header name (case-insensitive).
		if (starts_with_ci(name, "Content-Disposition"))
			parse_disposition(value, part);
		else if (starts_with_ci(name, "Content-Type"))
			part.content_type = value;
		// Other headers (Content-Transfer-Encoding etc.) are ignored.
	}
	return true;
}

// ============================================================
// parse: split body by boundary, extract each part
// ============================================================
// Wire format reminder:
//   --<boundary>\r\n
//   <headers>\r\n
//   \r\n
//   <body bytes>
//   \r\n--<boundary>...
//   ...
//   \r\n--<boundary>--\r\n
//
// Note the CRLF BEFORE each boundary belongs conceptually to the
// previous part's body terminator — we must strip it.
bool	MultipartParser::parse(const std::string& body,
							   const std::string& boundary)
{
	_parts.clear();
	_status = 0;

	if (boundary.empty())
	{
		_status = 400;
		return false;
	}

	// The actual marker on the wire is "--" + boundary value.
	std::string marker = "--" + boundary;

	// Find the first boundary. Per RFC, the body MAY begin with a CRLF
	// or some preamble; we just search for the first occurrence.
	size_t pos = body.find(marker);
	if (pos == std::string::npos)
	{
		// No boundary at all → either empty body or malformed.
		// Treat zero-length body as "no parts, no error."
		if (body.empty())
			return true;
		_status = 400;
		return false;
	}

	while (pos < body.size())
	{
		size_t after_marker = pos + marker.size();
		if (after_marker > body.size())
		{
			_status = 400;
			return false;
		}

		// Check for the terminating "--" right after the boundary —
		// that means "this was the LAST boundary, no more parts."
		if (after_marker + 1 < body.size()
			&& body[after_marker] == '-'
			&& body[after_marker + 1] == '-')
		{
			// All good — we've consumed every part.
			return true;
		}

		// Skip the CRLF (or LF) that follows the boundary line.
		size_t header_start = after_marker;
		if (header_start < body.size() && body[header_start] == '\r')
			++header_start;
		if (header_start < body.size() && body[header_start] == '\n')
			++header_start;
		else
		{
			// Boundary not followed by newline — malformed.
			_status = 400;
			return false;
		}

		// Find the blank line that separates headers from body.
		// We search for "\r\n\r\n" first; fall back to "\n\n" for
		// permissiveness (some hand-built tests use bare LF).
		size_t headers_end = body.find("\r\n\r\n", header_start);
		size_t body_start;
		if (headers_end != std::string::npos)
			body_start = headers_end + 4;
		else
		{
			headers_end = body.find("\n\n", header_start);
			if (headers_end == std::string::npos)
			{
				_status = 400;
				return false;
			}
			body_start = headers_end + 2;
		}

		// Find the NEXT boundary marker. The bytes between body_start
		// and the byte BEFORE the CRLF preceding the next boundary
		// are this part's payload.
		size_t next_marker = body.find(marker, body_start);
		if (next_marker == std::string::npos)
		{
			_status = 400;
			return false;
		}

		// Strip the CRLF that precedes the next boundary (it belongs
		// to the part terminator, not the data).
		size_t body_end = next_marker;
		if (body_end > 0 && body[body_end - 1] == '\n')
			--body_end;
		if (body_end > 0 && body[body_end - 1] == '\r')
			--body_end;
		if (body_end < body_start)
		{
			// Shouldn't happen — would mean a malformed empty part with
			// no separating CRLF. Treat as 400.
			_status = 400;
			return false;
		}

		MultipartPart part;
		std::string headers_block =
			body.substr(header_start, headers_end - header_start);
		if (!parse_part_headers(headers_block, part))
			return false;

		part.body = body.substr(body_start, body_end - body_start);
		_parts.push_back(part);

		pos = next_marker;
	}

	// We exited the loop without seeing a terminating "--". Some
	// clients are sloppy; we accept it as long as we got at least one
	// part. If parts is empty, fail.
	if (_parts.empty())
	{
		_status = 400;
		return false;
	}
	return true;
}
