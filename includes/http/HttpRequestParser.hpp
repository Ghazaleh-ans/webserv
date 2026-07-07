/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequestParser.hpp                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: mibrokhimov <contact@ibrokhimov.de>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/20 11:09:30 by gansari           #+#    #+#             */
/*   Updated: 2026/07/04 18:51:11 by mibrokhimov      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef HTTPREQUESTPARSER_HPP
# define HTTPREQUESTPARSER_HPP

# include <string>
# include "http/HttpRequest.hpp"

// Incremental HTTP/1.1 request parser
// The Client feeds it bytes as they arrive from recv()
// the parser advances its state machine and reports whether the request is complete, still incomplete, or broken

class HttpRequestParser
{
public:
	enum State
	{
		STATE_REQUEST_LINE,    // parsing "GET /foo HTTP/1.1"
		STATE_HEADERS,         // parsing header lines, ends at blank line
		STATE_BODY_LENGTH,     // reading Content-Length bytes
		STATE_CHUNK_SIZE,      // reading "1f\r\n" length line
		STATE_CHUNK_DATA,      // reading chunk body
		STATE_CHUNK_TRAILER,   // reading the final "0\r\n\r\n"
		STATE_DONE,            // entire request available in HttpRequest
		STATE_ERROR            // parse failed -> see status_code()
	};

	HttpRequestParser();
	~HttpRequestParser();

	void	reset();

	State	feed(const char* data, size_t len);

	State		state() const;
	int			status_code() const;
	const HttpRequest&	request() const;
	HttpRequest&		request();

	void	set_max_body_size(size_t bytes);

private:
	HttpRequest	_req;
	State		_state;
	int			_status_code;
	std::string	_buf;
	long	_body_remaining;
	long	_chunk_remaining;
	size_t	_headers_size_so_far;
	size_t	_max_header_size;
	size_t	_max_body_size;

	bool	parse_request_line();
	bool	parse_headers();
	bool	parse_body_length();
	bool	parse_chunk_size();
	bool	parse_chunk_data();
	bool	parse_chunk_trailer();

	void	fail(int code);
	bool	extract_line(std::string& out);
	void	split_uri();
	void	decide_post_header_state();
};

#endif
