/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: gansari <gansari@student.42berlin.de>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2026/05/19 13:03:46 by gansari           #+#    #+#             */
/*   Updated: 2026/05/19 13:03:48 by gansari          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <iostream>
#include <csignal>
#include "ConfigParser.hpp"
#include "Server.hpp"

// Signal handler for SIGINT/SIGTERM. Only allowed to do async-signal-safe
// work, which here is just setting a flag. The poll loop checks the flag
// each iteration.
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

	// Ignore SIGPIPE: when a client closes its end and we try to send,
	// the kernel would deliver SIGPIPE and kill the process. The
	// subject demands "must not crash under any circumstances," so
	// we ignore it — send() will return -1 instead, which the Client
	// code handles by closing the connection.
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
