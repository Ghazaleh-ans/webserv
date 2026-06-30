#pragma once

#include <string>
#include <fstream>
#include <sstream>

namespace extcfg
{

// Slurp a whole file into `out`. Returns false if the path can't be opened,
// true otherwise. Inline because the header lands in several translation units.
inline bool readFileToString(const std::string& path, std::string& out) {
	std::ifstream in(path.c_str());
	if (!in)
		return false;
	std::stringstream ss;
	ss << in.rdbuf();
	out = ss.str();
	return true;
}

} // namespace extcfg
