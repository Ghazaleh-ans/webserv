#ifndef MIMETYPES_HPP
# define MIMETYPES_HPP

# include <string>

namespace MimeTypes
{
	// MIME type = Multipurpose Internet Mail Extentions
	// Return the MIME type for a filesystem path based on extension
	std::string	from_path(const std::string& path);
}

#endif
