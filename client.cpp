//
// Simple chat client for TSAM-409
//
// Command line: ./chat_client 4000 
//
// Author: Jacky Mallett (jacky@ru.is)
//
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <algorithm>
#include <map>
#include <vector>
#include <thread>

#include <iostream>
#include <sstream>
#include <thread>
#include <map>
#include <chrono>
#include <ctime>

#include "utils.h"

// Threaded function for handling responss from server

void listenServer(int serverSocket)
{
	int nread; // Bytes read from socket
	char buffer[5000]; // Buffer for reading input

	while(true)
	{
		memset(buffer, 0, sizeof(buffer));
		nread = read(serverSocket, buffer, sizeof(buffer));

		auto now = std::chrono::system_clock::now();
		std::time_t now_c = std::chrono::system_clock::to_time_t(now);

		std::cout << "\nTimestamp: " << std::ctime(&now_c) << std::endl;

		if(nread == 0) // Server has dropped us
		{
			std::cout << "Over and Out" << std::endl;
			exit(0);
		}
		else if(nread > 0)
		{
			auto msg = parseMsg(buffer);
			if (msg.command == "TO_CLIENT")
			{
				std::cout << "\nReceived from server: \n" << msg.param << std::endl;
			}
		}
	}
}

int main(int argc, char* argv[])
{
	struct addrinfo hints, *svr; // Network host entry for server
	struct sockaddr_in serv_addr;  // Socket address for server
	int serverSocket;  // Socket used for server 
	int nwrite; // No. bytes written to server
	char buffer[5000]; // buffer for writing to server
	bool finished;
	int set = 1; // Toggle for setsockopt

	if(argc != 3)
	{
		std::cout << "Usage: " << "Client" << " <ip> <port>" << std::endl;
		std::cout << "Ctrl c to terminate" << std::endl;
		exit(0);
	}

	hints.ai_family	= AF_INET;	// IPv4 only addresses
	hints.ai_socktype = SOCK_STREAM;

	memset(&hints,	0, sizeof(hints));

	if(getaddrinfo(argv[1], argv[2], &hints, &svr) != 0)
	{
		perror("getaddrinfo failed: ");
		exit(0);
	}

	struct hostent *server;
	server = gethostbyname(argv[1]);

	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr,
		(char *)&serv_addr.sin_addr.s_addr,
		server->h_length);
	serv_addr.sin_port = htons(atoi(argv[2]));

	serverSocket = socket(AF_INET, SOCK_STREAM, 0);

	// Turn on SO_REUSEADDR to allow socket to be quickly reused after 
	// program exit.

	if(setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)) < 0)
	{
		std::cout << "Failed to set SO_REUSEADDR for port " << argv[2] << std::endl;
		perror("setsockopt failed: ");
	}

	
	if(connect(serverSocket, (struct sockaddr *)&serv_addr, sizeof(serv_addr) )< 0)
	{
		// EINPROGRESS means that the connection is still being setup. Typically this
		// only occurs with non-blocking sockets. (The serverSocket above is explicitly
		// not in non-blocking mode, so this check here is just an example of how to
		// handle this properly.)
		if(errno != EINPROGRESS)
		{
			std::cout << "Failed to open socket to server: " << argv[1] << " on port " << argv[2] << std::endl;
			perror("Connect failed: ");
			exit(0);
		}
	}

	// Listen and print replies from server
	std::thread serverThread(listenServer, serverSocket);

	for(;;)
	{
		std::string msg = "";
		bzero(buffer, sizeof(buffer));

		fgets(buffer, sizeof(buffer), stdin);

		msg += buffer;
		msg = createMsg(msg);

		std::cout << "Message: " << msg << std::endl;

		nwrite = send(serverSocket, msg.c_str(), msg.length(), 0);

		if(nwrite	== -1)
		{
			perror("send() to server failed: ");
			break;
		}

	}
}
	
