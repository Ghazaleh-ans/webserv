/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   PathUtils.hpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/26 13:45:29 by gansari           #+#    #+#             */
/*   Updated: 2026/07/01 18:10:36 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef PATHUTILS_HPP
# define PATHUTILS_HPP

# include <string>

namespace PathUtils
{
	
	std::string	lexical_normalize(const std::string& p);
	bool		is_within_root(const std::string& fs_path, const std::string& root);
}

#endif
