/* ************************************************************************** */
/*                                                                            */
/*   ConfigAdapter.hpp                                                        */
/*                                                                            */
/*   Bridges the imported `extcfg` parser to this project's config structs.   */
/*   ConfigAdapter::load() runs the extcfg lexer/parser/validator on the      */
/*   given file, then converts the result into the std::vector<ServerConfig>  */
/*   that Server/Router/ResponseBuilder/CgiSession already consume.           */
/*                                                                            */
/* ************************************************************************** */

#ifndef CONFIGADAPTER_HPP
# define CONFIGADAPTER_HPP

# include <string>
# include <vector>
# include "config/ServerConfig.hpp"

namespace ConfigAdapter
{
	// Loads and converts. Throws std::runtime_error (extcfg::ConfigException
	// derives from std::exception) on any parse/validation failure, plus the
	// extra per-location checks this project relied on (root-or-redirect,
	// upload requires POST).
	std::vector<ServerConfig>	load(const std::string& path);
}

#endif
