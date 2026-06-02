#ifndef MIMETYPES_HPP
# define MIMETYPES_HPP

# include <string>

// Static MIME-type lookup. Pure functions, no state. A namespace
// rather than a class because there's nothing to instantiate.
namespace MimeTypes
{
	// Return the MIME type for a filesystem path based on extension.
	// Falls back to "application/octet-stream" (the universal "unknown
	// binary blob" type) for unrecognised extensions — RFC 7231 §3.1.1.5
	// makes this the recommended default.
	std::string	from_path(const std::string& path);
}

#endif
