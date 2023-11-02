// Authors: Axel Ingi & Smári Brynjarsson 
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
#include <fcntl.h>

#include <string.h>
#include <algorithm>
#include <map>
#include <vector>
#include <list>
#include <chrono>

#include <iostream>
#include <sstream>
#include <thread>
#include <map>

#include <unistd.h>

#include "utils.h"


// fix SOCK_NONBLOCK for OSX
#ifndef SOCK_NONBLOCK
#include <fcntl.h>
#define SOCK_NONBLOCK O_NONBLOCK
#endif

const std::string GROUP_ID = "P3_GROUP_56"; // Group ID for this chat server
#define BACKLOG  5		// Allowed length of queue of waiting connections
#define MAX_SERVERS 10
#define MIN_SERVERS 5
#define MAX_CLIENTS 15

// simple class for halding messages
class Message 
{
	public:
		std::string toGroupID;
		std::string fromGroupID;
		std::string messageContent;
};

// Simple class for handling connections from clients.
//
// Client(int socket) - socket to send/receive traffic from client.
class Connection
{
	public:
		Connection() {} 
		Connection(int socket) : sock(socket){} 

		// closes the connection and marks the socket as -1
		void close() { 
			if (sock >= 0) {
				::close(sock);
				sock = -1;
			}
		}

		bool is_alive() const {
			return sock >= 0;
		}


		int sock{-1};		   // socket of client connection
		bool is_server{false};  // if true this is another server connecting to us
		std::string name;	   // Limit length of name of Connection's user
		std::string group_id = "N/A";
		
		std::string ip_address;
		std::string port;
		  
};

// Note: map is not necessarily the most efficient method to use here,
// especially for a server with large numbers of simulataneous connections,
// where performance is also expected to be an issue.
//
// Quite often a simple array can be used as a lookup table, 
// (indexed on socket no.) sacrificing memory for speed.

std::map<int, Connection> connections; // Lookup table for per Client information
std::map<std::string, std::vector<Message>> messageInbox;

// Open socket for specified port.
//
// Returns -1 if unable to create the socket for any reason.

int open_socket(int portno)
{
	struct sockaddr_in sk_addr; // address settings for bind()
	int sock;					// socket opened for this port
	int set = 1;				  // for setsockopt

	// Create socket for connection. Set to be non-blocking, so recv will
	// return immediately if there isn't anything waiting to be read.
#ifdef __APPLE__	 
	if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("Failed to open socket");
		return(-1);
	}
#else
	if((sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0)
	{
		perror("Failed to open socket");
		return(-1);
	}
#endif

	// Turn on SO_REUSEADDR to allow socket to be quickly reused after 
	// program exit.

	if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)) < 0)
	{
		perror("Failed to set SO_REUSEADDR:");
	}
	set = 1;
#ifdef __APPLE__	 
	if(setsockopt(sock, SOL_SOCKET, SOCK_NONBLOCK, &set, sizeof(set)) < 0)
	{
		perror("Failed to set SOCK_NOBBLOCK");
	}
#endif
	memset(&sk_addr, 0, sizeof(sk_addr));

	sk_addr.sin_family	= AF_INET;
	sk_addr.sin_addr.s_addr = inet_addr("0.0.0.0");
	sk_addr.sin_port		= htons(portno);

	// Bind to socket to listen for connections from clients

	if(bind(sock, (struct sockaddr *)&sk_addr, sizeof(sk_addr)) < 0)
	{
		perror("Failed to bind to socket:");
		return(-1);
	}
	else
	{
		return(sock);
	}
}

// connect to server
int connectServer(const std::string& group_id, const std::string& serverAddr, int serverPort)
{
	if (serverPort < 0 || serverPort > 65535)
	{
		perror("Could not connect to server: Invalid port number");
		return -1;
	}
	
	// Setup server connection
	struct sockaddr_in server_addr;

	// Create socket for connection.
	int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if(serverSocket < 0)
	{
		perror("Failed to open socket");
		return -1;
	}

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(serverPort);

	if (inet_pton(AF_INET, serverAddr.c_str(), (struct sockaddr *)&server_addr.sin_addr) <= 0)
	{
		perror("server address invalid");
		return -1;
	}

	// set socket to non-blocking to be able to control timeouts
	fcntl(serverSocket, F_SETFL, O_NONBLOCK);

	connect(serverSocket, (struct sockaddr *)&server_addr, sizeof(server_addr));

	fd_set fdset;
	FD_ZERO(&fdset);
	FD_SET(serverSocket, &fdset);

	struct timeval tv{ .tv_sec=10, .tv_usec=0};

	if (select(serverSocket + 1, NULL, &fdset, NULL, &tv) != 1)
	{
		close(serverSocket);
		return -1;
	}

	// Check for errors in socket
	int so_error;
	socklen_t len = sizeof so_error;

	getsockopt(serverSocket, SOL_SOCKET, SO_ERROR, &so_error, &len);

	if (so_error != 0) 
	{	
		close(serverSocket);
    	return -1;
	}

	// add connection to list of connections
	Connection newServer(serverSocket);
	newServer.group_id = group_id;
	newServer.ip_address = serverAddr;
	newServer.is_server = true;
	newServer.port = std::to_string(serverPort);
	connections[serverSocket] = newServer;

	std::cout << "Connected to server on address: " << serverAddr << " port: " << serverPort << std::endl;

	// send a message to the server to tell it our group id
	std::string msg = "QUERYSERVERS,P3_GROUP_56";
	msg = createMsg(msg);
	send(serverSocket, msg.c_str(), msg.length(), 0);

	return serverSocket;
}

std::vector<std::string> getServerStringList()
{
	std::vector<std::string> serverList;

	for(auto const& server : connections)
	{
		if (server.second.is_server && server.second.is_alive())
		{
			serverList.push_back(server.second.group_id + "," + server.second.ip_address + "," + server.second.port);
		}
	}

	return serverList;
}

// housekeeping for servers
void serverHousekeeping()
{
	// count number of servers we are connected to
	int nof_server = std::count_if(connections.begin(), connections.end(), [](const std::pair<int, Connection>& c) { return c.second.is_server; });


	//query other servers for their list of servers if we are connected to less than 6 servers
	if (nof_server < MIN_SERVERS)
	{
			for(auto const& server : connections)
			{
				if (server.second.is_server && server.second.is_alive())
				{
					std::string msg = "QUERYSERVERS,P3_GROUP_56";
					msg = createMsg(msg);
					if(send(server.second.sock, msg.c_str(), msg.length(), 0) < 0)
					{
						perror("Failed to send message to server");
					}
				}
			}
		
	}

	//compare to our list of servers for each time we get a list of servers from another server

	//if server is not in our list, try to connect to it

	// until we are connected to 6 servers
}

void housekeeping(const std::vector<std::string>& otherServerList)
{
	// count number of servers we are connected to
	int nof_server = std::count_if(connections.begin(), connections.end(), [](const std::pair<int, Connection>& c) { return c.second.is_server; });
	std::cout << "NOF Servers: " << nof_server << std::endl;
	for (auto const &server : otherServerList)
	{
		if (nof_server >= MAX_SERVERS)
		{
			break;
		}

		std::vector<std::string> ourServerList = getServerStringList();


		// check if we are already connected to this server
		if(std::find(ourServerList.begin(), ourServerList.end(), server) == ourServerList.end())
		{
			std::cout << "Trying to connect to server: <" << server << ">" << std::endl;

			auto tokens = split_string(server, ',');

			// NOTE we should do more validation here
			if (tokens.size() < 3) {
				std::cout << "Server list not formatted correctly. Ignoring" << std::endl;
			} 
			else if (tokens[0] != GROUP_ID)
			{
				if (connectServer(tokens[0], tokens[1], std::stoi(tokens[2])) >= 0)
				{
					nof_server++;
				}
			}
		}
	}
}

void keepAlive()
{
	for(auto const& server : connections)
	{
		if (server.second.is_server && server.second.is_alive())
		{
			int nof_messages = 0;

			if (messageInbox.find(server.second.group_id) != messageInbox.end())
			{
				nof_messages = messageInbox[server.second.group_id].size();
			}

			std::string msg = "KEEPALIVE," + std::to_string(nof_messages);
			msg = createMsg(msg);
			send(server.second.sock, msg.c_str(), msg.length(), 0);
		}
	}
}

void sendMsgToAllClients(std::string msg)
{
	std::string cliMsg = "TO_CLIENT,Sent from server: " + msg;
	for (auto const& con : connections)
	{
		if (!con.second.is_server && con.second.is_alive())
		{
			cliMsg = createMsg(cliMsg);
			send(con.second.sock, cliMsg.c_str(), cliMsg.length(), 0);
		}
	}
}

void sendMsgToAllServers()
{
	std::string toClients = "SERVERS: ";
	for (auto const& server : connections)
	{
		if (server.second.is_server && server.second.is_alive())
		{
			toClients += server.second.group_id + " - ";
			std::string msg = "SEND_MSG," + server.second.group_id + "," + GROUP_ID + "," + "Hello from Group 56";
			msg = createMsg(msg);
			send(server.second.sock, msg.c_str(), msg.length(), 0);
		}
	}
	toClients += "\nMessage: 'Hello from Group 56'";
	sendMsgToAllClients(toClients);
}

void serverCommand(int serverSocket, char *buffer)
{
	auto msg = parseMsg(buffer);
	if (msg.command == "")
	{
		return;
	}

	if (msg.command == "SERVERS")
	{
		auto newTokens = split_string(msg.param, ';');
		housekeeping(newTokens);
	}
	

	else if (msg.command == "QUERYSERVERS")
	{
		if (msg.param.empty() || split_string(msg.param, ',').size() > 1)
		{
			std::cout << "QUERYSERVERS: Message not formatted correctly ip: " << connections[serverSocket].ip_address << std::endl;
			connections[serverSocket].close();
			return;
		}
		int requesterSocket = serverSocket;

		// we use the socket that sent the request to
		// look up the corresponding Connection object.
		auto it = connections.find(requesterSocket);
		if(it == connections.end()) {
			// handle error, no such socket in connections
			std::cout << "No such socket in connections" << std::endl;
		}

		if (it->second.is_alive() == false)
		{
			std::cout << "Socket is not alive" << std::endl;
			return;
		}

		connections[requesterSocket].group_id = msg.param;

		Connection& requesterConnection = it->second;
		std::string resp = "SERVERS,";

		// add the requester's server details first
		// because requesters are always first in the list
		resp += requesterConnection.group_id + ",";
		resp += requesterConnection.ip_address + ",";
		resp += requesterConnection.port + ";";

		// add the details of the other servers
		for(auto const& server : connections)
		{
			if (server.second.is_server && server.first != requesterSocket && server.second.is_alive())
			{
				resp += server.second.group_id + ",";
				resp += server.second.ip_address + ",";
				resp += server.second.port + ";";
			}
		}
		resp = createMsg(resp);
		send(serverSocket, resp.c_str(), resp.length(), 0);
	}
	else if (msg.command == "KEEPALIVE")
	{
		std::string fetchMsg = "FETCH_MSGS," + GROUP_ID;
		fetchMsg = createMsg(fetchMsg);
		send(serverSocket, fetchMsg.c_str(), fetchMsg.length(), 0);
	}
	else if (msg.command == "SEND_MSG")
	{
		auto params = split_string(msg.param, ',');

		if (params.size() < 3)
		{
			std::cout << "Message not formatted correctly" << std::endl;
			return;
		}

		if (params[0] == GROUP_ID)
		{
			// Store until fetched or print straight away
			Message newMsg;
			newMsg.toGroupID = params[0];
			newMsg.fromGroupID = params[1];
			newMsg.messageContent = params[2];
			messageInbox[GROUP_ID].push_back(newMsg);

			for (auto const& server : connections)
			{
				if (!server.second.is_server && server.second.is_alive())
				{
					std::string msg = "From: " + connections[serverSocket].group_id + "\n";
					msg += "Message: " + params[2];
					send(server.second.sock, msg.c_str(), msg.length(), 0);
				}
			}
		}
		else
		{
			// Forward the message to the other servers
			bool messageSent = false;
			for(auto const& server : connections)
			{
				if (server.second.is_server && server.first != serverSocket && server.second.is_alive())
				{
					std::string msg = "SEND_MSG," + params[0] + "," + params[1] + "," + params[2];
					msg = createMsg(msg);
					send(server.second.sock, msg.c_str(), msg.length(), 0);
					messageSent = true;
				}
			}
			if (!messageSent)
			{
				Message newMsg;
				newMsg.toGroupID = params[0];
				newMsg.fromGroupID = params[1];
				newMsg.messageContent = params[2];

				// Add the message to the inbox of the specified group
				messageInbox[newMsg.toGroupID].push_back(newMsg);
			}
			else
			{
				std::string toClient = "Message forwarded to other servers: " + params[2];
				sendMsgToAllClients(toClient); 
			}
		}
	}
	else if (msg.command == "FETCH_MSGS")
	{
		auto params = split_string(msg.param, ',');
		if (params.size() < 1)
		{
			std::cout << "Message not formatted correctly" << std::endl;
			return;
		}

		std::string targetGroupID = params[0];

		auto it = messageInbox.find(targetGroupID);

		if(it == messageInbox.end()) {
			// handle error, no such group in messageInbox
			std::cout << "No message stored for group " << targetGroupID << std::endl;
		}
		else
		{
			std::vector<Message>& messages = it->second;

			// Generate the message string
			std::string msg = "SEND_MSG," + targetGroupID + ",";
			msg += messages[0].fromGroupID + "," + messages[0].messageContent;
			std::string toClient = "Message fetched from server: " + msg;
			
			msg = createMsg(msg);
			send(serverSocket, msg.c_str(), msg.length(), 0);
			sendMsgToAllClients(toClient);

			// Remove the message from the inbox
			if (!messages.empty())
			{
				messages.erase(messages.begin());
			}
		}
	}
	// Handling the STATUSREQ command
	else if (msg.command == "STATUSREQ")
	{
		std::string fromGroup = msg.param;

		// Generate the STATUSRESP
		std::string msg = "STATUSRESP," + GROUP_ID + "," + fromGroup;

		for (const auto& iter : messageInbox)
		{
			msg += "," + iter.first + "," + std::to_string(iter.second.size());
		}

		msg = createMsg(msg);
		send(serverSocket, msg.c_str(), msg.length(), 0);
	}
	else if (msg.command == "STATUSRESP")
	{
		std::string toGroup = msg.param;

		std::string msg = "STATUSRESP," + GROUP_ID + "," + toGroup;

		for(const auto& iter : messageInbox)
		{
			msg += "," + iter.first + "," + std::to_string(iter.second.size());
		}

		for(const auto& server : connections)
		{
			if (!server.second.is_server && server.second.is_alive())
			{
				msg = createMsg(msg);
				send(server.second.sock, msg.c_str(), msg.length(), 0);
			}
		}
	}
	else
	{
		std::cout << "Unknown command from server:" << buffer << std::endl;
	}
}


// Process command from client on the server

void clientCommand(int clientSocket, char *buffer) 
{
	auto msg = parseMsg(buffer);

	if(msg.command == "GETMSG")
	{
		auto params = split_string(msg.param, ',');

		std::string toGroupID = params[0];

		auto it = messageInbox.find(params[0]);

		if(it == messageInbox.end()) {
			// handle error, no such group in messageInbox
			std::cout << "No message stored for group " << toGroupID << std::endl;
		}
		else
		{
			std::vector<Message>& messages = it->second;

			// Generate the message string
			std::string msg = "TO_CLIENT,SENDMSG," + toGroupID + ",";
			msg += messages[0].fromGroupID + "," + messages[0].messageContent;

			msg = createMsg(msg);

			send(clientSocket, msg.c_str(), msg.length(), 0);

			// Remove the message from the inbox
			if (!messages.empty())
			{
				messages.erase(messages.begin());
			}
		}

	}
	else if(msg.command == "SENDMSG")
	{
		auto params = split_string(msg.param, ',');

		if (params.size() != 2)
		{
			std::cout << "Message not formatted correctly" << std::endl;
			return;
		}

		if (params[0] == GROUP_ID)
		{
			Message newMsg;
			newMsg.toGroupID = GROUP_ID;
			newMsg.messageContent = params[1];
			messageInbox[GROUP_ID].push_back(newMsg);
		}
		else
		{
			// Forward the message to the other servers
			bool messageSent = false;
			for(auto const& server : connections)
			{
				if (server.second.is_server && server.second.group_id == params[0] && server.second.is_alive())
				{
					std::string msg = "SEND_MSG," + params[0] + "," + GROUP_ID + "," + params[1];
					msg = createMsg(msg);
					send(server.second.sock, msg.c_str(), msg.length(), 0);
					
					std::string toClient = "Message sent to " + server.second.group_id + " Message: " + params[1];
					sendMsgToAllClients(toClient);
					messageSent = true;
				}
			}
			if (!messageSent)
			{
				Message newMsg;
				newMsg.toGroupID = params[0];
				newMsg.fromGroupID = GROUP_ID;
				newMsg.messageContent = params[1];

				// Add the message to the inbox of the specified group
				messageInbox[params[0]].push_back(newMsg);
			}
		}
		
	}
	else if(msg.command == "LISTSERVERS")
	{
		std::string msg = "TO_CLIENT,SERVERS: ";

		for(auto const& names : connections)
		{
			if (names.second.is_server && names.second.is_alive())
			{
				msg += names.second.group_id + ",";
				msg += names.second.ip_address + ",";
				msg += names.second.port + ";";
			}
		}
		
		msg = createMsg(msg);
		send(clientSocket, msg.c_str(), msg.length(), 0);

	}
	else
	{
		std::cout << "Unknown command from client:" << buffer << std::endl;
	}
}


int main(int argc, char* argv[])
{

	if(argc != 3)
	{
		printf("Usage: chat_server <clientPort> <serverPort>\n");
		exit(0);
	}

	// Setup socket for server to listen to new client connections
	int clientListenSock = open_socket(atoi(argv[2])); 
	printf("Listening for clients on port: %d. Socket %i\n", atoi(argv[2]), clientListenSock);

	if(listen(clientListenSock, BACKLOG) < 0)
	{
		printf("Listen for clients failed on port %s\n", argv[2]);
		exit(0);
	}

	// Setup socket for server to listen to new server connections
	int serverListenSock = open_socket(atoi(argv[1])); 
	printf("Listening for servers on port: %d. Socket %i\n", atoi(argv[1]), serverListenSock);

	if(listen(serverListenSock, BACKLOG) < 0)
	{
		printf("Listen for servers failed on port %s\n", argv[1]);
		exit(0);
	}

	// Connect to instructor server
	connectServer("Instr_1", "130.208.243.61" , 4001);

	// Set timestamp for keepalive
	auto keepaliveTimestamp = std::chrono::system_clock::now();
	auto msgTimestamp = std::chrono::system_clock::now();

	for(;;)
	{
		fd_set readSockets;
		FD_ZERO(&readSockets);
		FD_SET(clientListenSock, &readSockets); // first add the listen socket
		FD_SET(serverListenSock, &readSockets); // then add the server socket
		int maxfds = std::max(clientListenSock, serverListenSock);


		// Now add the sockets for any connections we have open
		for(auto const& con : connections)
		{
			// printf("Adding socket %i\n", con.first);
			FD_SET(con.first, &readSockets);
			maxfds = std::max(maxfds, con.first);
		}

		// Look at sockets and see which ones have something to be read()
		struct timeval tv{ .tv_sec=10, .tv_usec=0};
		int n = select(maxfds + 1, &readSockets, NULL, NULL, &tv);

		// for debugging, check the status of all the sockets
		for(auto const& pair : connections)
		{
			const Connection& connection = pair.second;
			bool read_set = FD_ISSET(connection.sock, &readSockets);
			// bool except_set = false; // FD_ISSET(connection.sock, &exceptSockets);
			// if(read_set || except_set) {
			// 	std::cout << "socket " << connection.sock << " " << (read_set ? "READ " : "") << (except_set ? "EXCEPT" : "") << std::endl;
			// }
		}

		if(n < 0)
		{
			perror("select failed - closing down\n");
			break;
		}

		// First, accept any new client connections to the server on the listening socket
		if(FD_ISSET(clientListenSock, &readSockets))
		{
			if (std::count_if(connections.begin(), connections.end(), [](const std::pair<int, Connection>& c) { return c.second.is_server == false; }) < MAX_CLIENTS)
			{	
				struct sockaddr_in client_addr;
				socklen_t clientLen = sizeof(client_addr);
				
				memset(&client_addr, 0, sizeof(client_addr));

				int clientSock = accept(clientListenSock, (struct sockaddr *)&client_addr, &clientLen);
				if (clientSock <= 0) {
					printf("Error while trying to accept a client connection (%i)\n", errno);

				}
				else {
					printf("accepted*** new client connection on socket: %d\n", clientSock);

					// create a new client to store information.
					connections[clientSock] = Connection(clientSock);
				}
			}
			else
			{
				printf("Client connection refused, max number of clients reached\n");
			}
		}

		// Accept any new server connections to the server on the listening socket
		if(FD_ISSET(serverListenSock, &readSockets))
		{
			if (std::count_if(connections.begin(), connections.end(), [](const std::pair<int, Connection>& c) { return c.second.is_server; }) < MAX_SERVERS)
			{	
				struct sockaddr_in server_addr;
				socklen_t server_len = sizeof(server_addr);

				char ip[INET_ADDRSTRLEN];
				
				memset(&server_addr, 0, sizeof(server_addr));

				int serverSock = accept(serverListenSock, (struct sockaddr *)&server_addr, &server_len);
				if (serverSock <= 0) {
					printf("Error while trying to accept server a connection (%i)\n", errno);

				}
				else {
					printf("accepted*** new server conection on socket: %d\n", serverSock);

					// create a new client to store information.
					connections[serverSock] = Connection(serverSock);
					connections[serverSock].is_server = true;

					inet_ntop(AF_INET, &(server_addr.sin_addr), ip, INET_ADDRSTRLEN);
					connections[serverSock].ip_address = ip;
				}
			}
			else
			{
				printf("Server connection refused, max number of servers reached\n");
			}
		}

		// Now check for commands from clients
		for(auto &pair : connections)
		{
			Connection& connection = pair.second;

			if(FD_ISSET(connection.sock, &readSockets))
			{
				char buffer[5001];		  // buffer for reading from clients
				memset(buffer, 0, sizeof(buffer));
				

				// recv() == 0 means client has closed connection
				// reading only sizeof(buffer) -1 to make sure that our buffer is null terminated
				int bytesRead = recv(connection.sock, buffer, sizeof(buffer) - 1, 0);

				if(bytesRead == 0)
				{
					std::cout << "closing connection to socket: " << connection.sock << std::endl;
					connection.close();
				}
				else if(bytesRead < 0)
				{
					perror("recv failed");
					connection.close();
				}
				// We don't check for -1 (nothing received) because select()
				// only triggers if there is something on the socket for us.
				else
				{   
					if (connection.is_server)
					{
						serverCommand(connection.sock, buffer);
					}
					else
					{
						clientCommand(connection.sock, buffer);
					}
				}
			}
		}

		// Remove closed connections from the map
		for (auto it = connections.begin(); it != connections.end();)
		{
			if (it->second.sock < 0) {
				it = connections.erase(it);
			} else {
				++it;
			}
		}

		serverHousekeeping();
		
		if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - keepaliveTimestamp).count() > 60){
			// reset timer
			keepaliveTimestamp = std::chrono::system_clock::now();

			// send keepalive to all servers
			keepAlive();
		}

		if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - msgTimestamp).count() > 250){
			// reset timer
			msgTimestamp = std::chrono::system_clock::now();

			// send msg to all servers
			sendMsgToAllServers();
		}

	}

	// Close any open listening sockets
	close(clientListenSock);
	close(serverListenSock);
}
