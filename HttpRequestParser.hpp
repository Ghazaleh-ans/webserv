/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequestParser.hpp                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/20 11:09:30 by gansari           #+#    #+#             */
/*   Updated: 2026/05/20 11:09:32 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef HTTPREQUESTPARSER_HPP
# define HTTPREQUESTPARSER_HPP

# include <string>
# include "HttpRequest.hpp"

// Incremental HTTP/1.1 request parser. The Client feeds it bytes as
// they arrive from recv(); the parser advances its state machine and
// reports whether the request is complete, still incomplete, or broken.
//
// Design: each feed() call processes as much of the input as it can,
// then returns. State is preserved in member variables, so partial
// reads are handled transparently — feed("GET /") then feed(" HTTP/1.1\r\n")
// produces the same result as feed("GET / HTTP/1.1\r\n").
class HttpRequestParser
{
public:
	// HTTP status codes the parser can decide on its own. 200 means
	// "no error yet"; anything else means "fail the request with this
	// status." Reported via status_code() once state == ERROR.
	enum State
	{
		STATE_REQUEST_LINE,    // parsing "GET /foo HTTP/1.1"
		STATE_HEADERS,         // parsing header lines, ends at blank line
		STATE_BODY_LENGTH,     // reading Content-Length bytes
		STATE_CHUNK_SIZE,      // reading "1f\r\n" length line
		STATE_CHUNK_DATA,      // reading chunk body
		STATE_CHUNK_TRAILER,   // reading the final "0\r\n\r\n"
		STATE_DONE,            // entire request available in HttpRequest
		STATE_ERROR            // parse failed; see status_code()
	};

	HttpRequestParser();
	~HttpRequestParser();

	// Reset for parsing a new request on the same connection.
	void	reset();

	// Feed bytes from recv(). Returns the current state after consuming
	// them. Caller should typically loop: feed → check state == DONE
	// or ERROR → act. We process every byte we can each call.
	State	feed(const char* data, size_t len);

	// Public accessors
	State		state() const;
	int			status_code() const;
	const HttpRequest&	request() const;
	HttpRequest&		request();

	// Configurable limits. Defaults match common server practice.
	// The Client sets body_max from the matching ServerConfig at runtime.
	void	set_max_header_size(size_t bytes);   // total bytes of all headers
	void	set_max_body_size(size_t bytes);     // Content-Length cap

private:
	HttpRequest	_req;
	State		_state;
	int			_status_code;

	// Accumulator buffer: bytes fed in but not yet consumed by the
	// state machine. We append to it on feed(), then the state machine
	// drains it. This handles the "half a line per recv" case.
	std::string	_buf;

	// While in STATE_BODY_LENGTH, how many bytes still to read.
	long	_body_remaining;

	// While in STATE_CHUNK_DATA, how many bytes of the current chunk left.
	long	_chunk_remaining;

	// Cumulative size of headers parsed so far (for the max_header_size cap).
	size_t	_headers_size_so_far;

	// Configurable caps.
	size_t	_max_header_size;
	size_t	_max_body_size;

	// --- State-machine handlers, each returns true if it made progress
	//     (and may have transitioned state), false if it needs more data. ---
	bool	parse_request_line();
	bool	parse_headers();
	bool	parse_body_length();
	bool	parse_chunk_size();
	bool	parse_chunk_data();
	bool	parse_chunk_trailer();

	// Helpers
	void	fail(int code);
	bool	extract_line(std::string& out);  // pop one CRLF-terminated line from _buf
	void	split_uri();                     // split _req.uri into path+query
	void	decide_post_header_state();      // after blank line, where next?
};

#endif
