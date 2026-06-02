#include "MimeTypes.hpp"
#include <cctype>

namespace MimeTypes
{

// Small hand-curated table covering the extensions a project-grade web
// server actually meets. NGINX has ~80 entries; the full IANA list has
// thousands. We pick the ones a browser actually sends/receives during
// normal testing. Unknown → octet-stream.
//
// Ordered by rough frequency (HTML first, plain text last) for cache
// friendliness when comparing string-by-string. The table is tiny so
// linear scan is fine.
struct Entry
{
	const char*	ext;
	const char*	type;
};

static const Entry	g_table[] = {
	{ "html", "text/html; charset=utf-8" },
	{ "htm",  "text/html; charset=utf-8" },
	{ "css",  "text/css" },
	{ "js",   "application/javascript" },
	{ "json", "application/json" },
	{ "xml",  "application/xml" },
	{ "txt",  "text/plain; charset=utf-8" },
	{ "md",   "text/plain; charset=utf-8" },

	{ "png",  "image/png" },
	{ "jpg",  "image/jpeg" },
	{ "jpeg", "image/jpeg" },
	{ "gif",  "image/gif" },
	{ "svg",  "image/svg+xml" },
	{ "ico",  "image/x-icon" },
	{ "webp", "image/webp" },

	{ "pdf",  "application/pdf" },
	{ "zip",  "application/zip" },
	{ "gz",   "application/gzip" },
	{ "tar",  "application/x-tar" },

	{ "mp3",  "audio/mpeg" },
	{ "mp4",  "video/mp4" },
	{ "wav",  "audio/wav" },

	{ "woff", "font/woff" },
	{ "woff2","font/woff2" },
	{ "ttf",  "font/ttf" },

	{ NULL, NULL }
};

// Lowercase a string in place. Helper for case-insensitive extension
// matching ("README.HTML" should be served as text/html).
static std::string	to_lower(const std::string& s)
{
	std::string out = s;
	for (size_t i = 0; i < out.size(); ++i)
		out[i] = static_cast<char>(std::tolower(
			static_cast<unsigned char>(out[i])));
	return out;
}

std::string	from_path(const std::string& path)
{
	// Find the last '.' AFTER the last '/'. Otherwise "/foo.bar/baz"
	// (a file called "baz" inside a directory with a dot in its name)
	// would be misread as having extension "bar/baz".
	size_t slash = path.find_last_of('/');
	size_t dot = path.find_last_of('.');
	if (dot == std::string::npos)
		return "application/octet-stream";
	if (slash != std::string::npos && dot < slash)
		return "application/octet-stream";

	std::string ext = to_lower(path.substr(dot + 1));

	for (size_t i = 0; g_table[i].ext != NULL; ++i)
	{
		if (ext == g_table[i].ext)
			return g_table[i].type;
	}
	return "application/octet-stream";
}

}  // namespace MimeTypes
