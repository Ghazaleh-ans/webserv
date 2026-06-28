/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/19 13:03:46 by gansari           #+#    #+#             */
/*   Updated: 2026/06/28 13:52:34 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <iostream>
#include <csignal>
#include "config/ConfigParser.hpp"
#include "core/Server.hpp"

static void	handle_signal(int /*sig*/)
{
	Server::request_stop();
}

int	main(int argc, char** argv)
{
	if (argc != 2)
	{
		std::cerr << "usage: " << argv[0] << " <config-file>\n";
		return 1;
	}

	// SIGPIPE: a synchronous signal that's sent to a process
	// which attempts to write data to a socket or pipe that has been closed by the reading end
	std::signal(SIGPIPE, SIG_IGN);

	// SIGINT (Ctrl-C) and SIGTERM cleanly stop the loop.
	std::signal(SIGINT, handle_signal);
	std::signal(SIGTERM, handle_signal);

	try
	{
		ConfigParser parser;
		std::vector<ServerConfig> configs = parser.parse_file(argv[1]);

		Server server(configs);
		server.start();
		server.run();

		return 0;
	}
	catch (const std::exception& e)
	{
		std::cerr << "ERROR: " << e.what() << "\n";
		return 1;
	}
}
