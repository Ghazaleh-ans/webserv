/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   MultipartParser.hpp                                :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/03 16:57:17 by gansari           #+#    #+#             */
/*   Updated: 2026/06/23 16:23:09 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef MULTIPARTPARSER_HPP
# define MULTIPARTPARSER_HPP

# include <string>
# include <vector>
# include <map>

// A part with a non-empty `filename` -> file upload
// A part with `filename` empty -> form field
struct MultipartPart
{
	std::string	name;       // form field name from Content-Disposition
	std::string	filename;   // filename from Content-Disposition (empty if not a file)
	std::string	content_type;  // from Content-Type header, e.g. "image/png"
	std::string	body;       // raw bytes of this part (may be binary)
};

// Parses a multipart/form-data body in one shot
// The HTTP parser already collected the full body (Content-Length or chunked-then-
// un-chunked)
//
// One-shot rather than incremental because the boundary string can
// only be known after headers are parsed, and by that point the
// HttpRequestParser already has the whole body buffered.
class MultipartParser
{
public:
	MultipartParser();
	~MultipartParser();

	// Parse `body` using the boundary value extracted from the
	// Content-Type header (without the leading "--").
	// Returns true on success; on false, status_code() tells you
	// which HTTP code to emit.
	bool	parse(const std::string& body, const std::string& boundary);

	// 0 if parse succeeded; HTTP status code (400, 413, ...) otherwise.
	int		status_code() const;

	const std::vector<MultipartPart>&	parts() const;

	// Extract just the boundary value from a Content-Type header value.
	// Returns empty string if the header isn't multipart/form-data or
	// boundary= is missing.
	// Header example: "multipart/form-data; boundary=----WebKit123"
	static std::string	extract_boundary(const std::string& content_type);

private:
	std::vector<MultipartPart>	_parts;
	int							_status;

	// Parse the headers section of one part. Each part starts with
	// header lines (Content-Disposition, Content-Type) then a blank
	// line then bytes. We populate `part` from those headers.
	bool	parse_part_headers(const std::string& headers,
								MultipartPart& part);

	// Extract `name` and `filename` from a Content-Disposition line.
	// e.g. 'form-data; name="upload"; filename="x.txt"'
	void	parse_disposition(const std::string& value,
							  MultipartPart& part);

	// Helper: case-insensitive prefix match.
	static bool	starts_with_ci(const std::string& s, const std::string& prefix);
};

#endif
