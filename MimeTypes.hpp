/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   MimeTypes.hpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/06/19 11:55:35 by gansari           #+#    #+#             */
/*   Updated: 2026/06/19 11:55:36 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

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
