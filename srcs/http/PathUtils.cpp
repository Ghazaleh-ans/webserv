/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   PathUtils.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/26 13:47:12 by gansari           #+#    #+#             */
/*   Updated: 2026/06/26 13:58:22 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "http/PathUtils.hpp"
#include <vector>

// Resolve . and .. segments purely by string manipulation (no filesystem access)
// Splits on '/', drops "." segments, pops the previous segment on "..",
// and preserves leading ".." only for relative paths (absolute paths can't go above /)
std::string	PathUtils::lexical_normalize(const std::string& p)
{
	bool is_abs = (!p.empty() && p[0] == '/');

	std::vector<std::string> parts;
	size_t i = 0;
	while (i < p.size())
	{
		// Skip slash runs
		while (i < p.size() && p[i] == '/')
			++i;
		size_t start = i;
		while (i < p.size() && p[i] != '/')
			++i;
		// To avoid empty string from being added to parts
		// Example: /foo/bar/// -> parts = [foo, bar]
		if (start == i)
			break;
		std::string seg = p.substr(start, i - start);
		if (seg == ".")
			continue;
		if (seg == "..")
		{
			if (!parts.empty() && parts.back() != "..")
				parts.pop_back();
			else if (!is_abs)
				parts.push_back("..");
			// else: ".." above root, silently drop (means "stay at /")
			continue;
		}
		parts.push_back(seg);
	}

	std::string out;
	if (is_abs)
		out = "/";
	for (size_t j = 0; j < parts.size(); ++j)
	{
		if (j > 0 || !is_abs)
		{
			if (!out.empty() && out[out.size() - 1] != '/')
				out += '/';
		}
		out += parts[j];
	}
	if (out.empty())
		out = is_abs ? "/" : ".";
	return out;
}

// Returns true if fs_path stays inside root after resolving . and .. -> blocks path traversal
bool	PathUtils::is_within_root(const std::string& fs_path, const std::string& root)
{
	if (root.empty())
		return true; // no root configured -> can't enforce -> let it through
	std::string norm_path = lexical_normalize(fs_path);
	std::string norm_root = lexical_normalize(root);

	if (!norm_root.empty() && norm_root[norm_root.size() - 1] == '/' && norm_root.size() > 1)
		norm_root.resize(norm_root.size() - 1);
	if (norm_path == norm_root)
		return true;
	if (norm_path.size() > norm_root.size() && norm_path.compare(0, norm_root.size(), norm_root) == 0 && norm_path[norm_root.size()] == '/')
		return true;
	return false;
}
