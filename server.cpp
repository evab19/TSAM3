//
// Simple chat server for TSAM-409
//
// Command line: ./chat_server 4000
//
// Author: Jacky Mallett (jacky@ru.is)
//
// Modified by: Eva Ösp Björnsdóttir (evab19@ru.is)
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
#include <list>

#include <iostream>
#include <sstream>
#include <thread>
#include <map>

#include <unistd.h>

// fix SOCK_NONBLOCK for OSX
#ifndef SOCK_NONBLOCK
#include <fcntl.h>
#define SOCK_NONBLOCK O_NONBLOCK
#endif

#define BACKLOG 5 // Allowed length of queue of waiting connections

// Simple class for handling connections from clients.
//
// Client(int socket) - socket to send/receive traffic from client.
class Client
{
public:
    int sock;                         // socket of client connection
    std::string name;                 // Limit length of name of client's user
    int port;                         // port number of client connection
    std::string ip_address;           // ip address of client connection

    Client(int socket, int p, std::string ip)
    {
        sock = socket;
        port = p;
        ip_address = ip;
    }

    ~Client() {} // Virtual destructor defined for base class
};

// Note: map is not necessarily the most efficient method to use here,
// especially for a server with large numbers of simulataneous connections,
// where performance is also expected to be an issue.
//
// Quite often a simple array can be used as a lookup table,
// (indexed on socket no.) sacrificing memory for speed.

std::map<int, Client *> clients; // Lookup table for per Client information
std::map<std::string, std::map<std::string, std::vector<std::string>>> messagesPerGroup; // storing messages for groups

// Open socket for specified port.
//
// Returns -1 if unable to create the socket for any reason.

int open_socket(int portno)
{
    struct sockaddr_in sk_addr; // address settings for bind()
    int sock;                   // socket opened for this port
    int set = 1;                // for setsockopt

    // Create socket for connection. Set to be non-blocking, so recv will
    // return immediately if there isn't anything waiting to be read.
#ifdef __APPLE__
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Failed to open socket");
        return (-1);
    }
#else
    if ((sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0)
    {
        perror("Failed to open socket");
        return (-1);
    }
#endif

    // Turn on SO_REUSEADDR to allow socket to be quickly reused after
    // program exit.

    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)) < 0)
    {
        perror("Failed to set SO_REUSEADDR:");
    }
    set = 1;
#ifdef __APPLE__
    if (setsockopt(sock, SOL_SOCKET, SOCK_NONBLOCK, &set, sizeof(set)) < 0)
    {
        perror("Failed to set SOCK_NOBBLOCK");
    }
#endif
    memset(&sk_addr, 0, sizeof(sk_addr));

    sk_addr.sin_family = AF_INET;
    sk_addr.sin_addr.s_addr = INADDR_ANY;
    sk_addr.sin_port = htons(portno);

    // Bind to socket to listen for connections from clients

    if (bind(sock, (struct sockaddr *)&sk_addr, sizeof(sk_addr)) < 0)
    {
        perror("Failed to bind to socket:");
        return (-1);
    }
    else
    {
        return (sock);
    }
}

// Close a client's connection, remove it from the client list, and
// tidy up select sockets afterwards.

void closeClient(int clientSocket, fd_set *openSockets, int *maxfds)
{

    printf("Client closed connection: %d\n", clientSocket);

    // If this client's socket is maxfds then the next lowest
    // one has to be determined. Socket fd's can be reused by the Kernel,
    // so there aren't any nice ways to do this.

    close(clientSocket);

    if (*maxfds == clientSocket)
    {
        for (auto const &p : clients)
        {
            *maxfds = std::max(*maxfds, p.second->sock);
        }
    }

    // And remove from the list of open sockets.

    FD_CLR(clientSocket, openSockets);
}

void keepAlive()
{
    std::string msg;
    msg += "I don't want to go";
}

void connectClient(std::vector<std::string> tokens)
{
    struct addrinfo hints, *svr;  // Network host entry for server
    struct sockaddr_in serv_addr; // Socket address for server
    int serverSocket;             // Socket used for server
    int nwrite;                   // No. bytes written to server
    char buffer[4096];            // buffer for writing to server
    bool finished;
    int set = 1; // Toggle for setsockopt

    hints.ai_family = AF_INET; // IPv4 only addresses
    hints.ai_socktype = SOCK_STREAM;

    char *host = &(tokens[1])[0];
    char *port = &(tokens[2])[0];

    memset(&hints, 0, sizeof(hints));

    if (getaddrinfo(host, port, &hints, &svr) != 0)
    {
        perror("getaddrinfo failed: ");
        return;
    }

    struct hostent *server;
    server = gethostbyname(host);

    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
          (char *)&serv_addr.sin_addr.s_addr,
          server->h_length);
    serv_addr.sin_port = htons(atoi(port));

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    // Turn on SO_REUSEADDR to allow socket to be quickly reused after
    // program exit.

    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)) < 0)
    {
        printf("Failed to set SO_REUSEADDR for port %s\n", port);
        perror("setsockopt failed: ");
    }

    if (connect(serverSocket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        if (errno != EINPROGRESS)
        {
            printf("Failed to open socket to server: %s\n", host);
            perror("Connect failed: ");
            return;
        }
    }

    std::string msg = "*QUERYSERVERS,P3_GROUP_82#";
    send(serverSocket, msg.c_str(), msg.length(), 0);
    memset(buffer, 0, 4096);
        if (recv(serverSocket, buffer, 4096, 0) < 0) {
        perror("Did not receive a response\n");
        }
        else {
            std::cout << buffer << std::endl;
        }

}

// Process command from client on the server

void clientCommand(int clientSocket, fd_set *openSockets, int *maxfds,
                   char *buffer)
{
    // TODO: set it so it looks for * and # in the stream to be sure we have a full command
    std::vector<std::string> tokens;
    std::string token;

    if (buffer != NULL) {
        std::string buf(buffer);
        std::replace(buf.begin(), buf.end(), ',', ' ');

        // Split command from client into tokens for parsing
        std::stringstream stream(buf);

        while (stream >> token) {
            tokens.push_back(token);
        }
        

        if (!(buf[0] == '*' && buf[buf.size() - 1] == '#')) {
            std::string msg = "Commands must start with * and end with # " + std::string(buffer);
            send(clientSocket, msg.c_str(), msg.length(), 0);
        }   
        else {
            tokens[0].erase(0, 1);
            tokens[tokens.size() - 1] = tokens[tokens.size() - 1].substr(0, tokens[tokens.size() - 1].size()-1);

            if (tokens[0].compare("CONNECT") == 0)
            {
                if (tokens.size() != 3)
                {
                    perror("Connect requires an IP address and Port number\n");
                }
                else
                {
                    connectClient(tokens);
                }
            }
            else if (tokens[0].compare("LEAVE") == 0)
            {
                closeClient(clientSocket, openSockets, maxfds);
            }

            else if (tokens[0].compare("QUERYSERVERS") == 0)
            {
                if (tokens.size() == 2) {
                    // TODO order the response so the querying server is printed first!
                    std::cout << "Servers connected" << std::endl;
                    std::string msg;

                    clients[clientSocket]->name = tokens[1];
                    std::cout << tokens[1] << std::endl;

                    msg += "CONNECTED,";
                    for (auto const &sock : clients)
                    {
                        msg += sock.second->name + "," + sock.second->ip_address + "," + std::to_string(sock.second->port) + ";";
                    }

                    send(clientSocket, msg.c_str(), msg.length() - 1, 0);
                }
                
            }

            else if (tokens[0].compare("SEND_MSG") == 0)
            {
                std::string msg;
                for (auto i = tokens.begin() + 3; i != tokens.end(); i++)
                {
                    msg += *i + " ";
                }
                if (!msg.empty()) {
                    bool wasConnected = false;
                    for (auto const &sock : clients)
                    {
                        if (sock.second->name.compare(tokens[1]) == 0)
                        {
                            wasConnected = true;
                            send(sock.second->sock, msg.c_str(), msg.length(), 0);
                            break;
                        }
                    }
                    std::string reply = "Not connected to group server, storing message";
                    if (!wasConnected) {
                        // Store it instead
                        if (messagesPerGroup.count(tokens[1]) > 0) {
                            // We already have messages for the group
                            if (messagesPerGroup[tokens[1]].count(tokens[2]) > 0) {
                                // We already have messages for this group from the sender
                                messagesPerGroup.at(tokens[1]).at(tokens[2]).push_back(msg);
                            }
                            else {
                                // Receiver didn't already have messages from sender
                                std::vector<std::string> v;
                                v.push_back(msg);
                                messagesPerGroup[tokens[1]].insert(std::pair<std::string, std::vector<std::string>>(tokens[2], v));
                            }
                        }
                        else {
                            // Group hasn't had a message yet, create an instance for it in the map and a new vector
                            std::vector<std::string> v;
                            v.push_back(msg);
                            std::map<std::string, std::vector<std::string>> innerMap;
                            innerMap[tokens[2]] = v;
                            messagesPerGroup[tokens[1]] = innerMap;
                        }
                        send(clientSocket, reply.c_str(), reply.length(), 0);
                    }
                }
                else {
                    std::string reply = "Please don't send empty messages";
                    send(clientSocket, reply.c_str(), reply.length(), 0);
                }
                
            }
            else if (tokens[0].compare("GET_MSG") == 0) {
                std::string reply = "";
                std::map<std::string, std::vector<std::string>> groupMap;
                try {
                    groupMap = messagesPerGroup[tokens[1]];
                    for(std::map<std::string, std::vector<std::string>>::iterator i = groupMap.begin(); i != groupMap.end(); ++i) {
                        reply += "Messages from group " + i->first + "\n";
                        for (int j = 0; j < i->second.size(); j++) {
                            reply += i->second[j] + "\n";
                        }
                    }
                }
                catch (const std::out_of_range& e) {
                    // No messages found
                    
                }
                if (groupMap.empty()) {
                    reply = "No messages found for group " + tokens[1];
                }
                send(clientSocket, reply.c_str(), reply.length(), 0);
            }

            else if (tokens[0].compare("STATUSREQ") == 0) {
                if (tokens.size() == 2) {

                    std::string msg;

                    msg += "STATUSRESP,P3_GROUP_82," + tokens[1];
                    // Loop through the unique groups we have messages for
                    for(std::map<std::string, std::map<std::string, std::vector<std::string>>>::iterator i = messagesPerGroup.begin(); i != messagesPerGroup.end(); ++i) {
                        // Add group name
                        msg += "," + i->first + ",";
                        int count = 0;
                        // Add the number of messages from each group that has sent the current one messages
                        for(std::map<std::string, std::vector<std::string>>::iterator j = i->second.begin(); j != i->second.end(); ++j) {
                            count += j->second.size();
                        }
                        msg += std::to_string(count);
                    }

                    send(clientSocket, msg.c_str(), msg.length(), 0);
                }
            }

            else if (tokens[0].compare("LISTSERVERS") == 0) {
                    std::cout << "Servers connected" << std::endl;
                    std::string msg;

                    msg += "CONNECTED,";
                    for (auto const &sock : clients)
                    {
                        msg += sock.second->name + ", ";
                    }

                    send(clientSocket, msg.c_str(), msg.length() - 1, 0);
            }

            else
            {
                std::cout << "Unknown command from client:" << buffer << std::endl;
            }
        }
    }
    
}

int main(int argc, char *argv[])
{
    bool finished;
    int listenSock;       // Socket for connections to server
    int clientSock;       // Socket of connecting client
    fd_set openSockets;   // Current open sockets
    fd_set readSockets;   // Socket list for select()
    fd_set exceptSockets; // Exception socket list
    int maxfds;           // Passed to select() as max fd in set
    struct sockaddr_in client;
    socklen_t clientLen;
    char buffer[4096]; // buffer for reading from clients

    std::string group = "P3_Group_82";

    if (argc != 2)
    {
        printf("Usage: ./tsamgroup82 <ip port>\n");
        exit(0);
    }

    // Setup socket for server to listen to

    listenSock = open_socket(atoi(argv[1]));
    printf("Listening on port: %d\n", atoi(argv[1]));

    if (listen(listenSock, BACKLOG) < 0)
    {
        printf("Listen failed on port %s\n", argv[1]);
        exit(0);
    }
    else
    // Add listen socket to socket set we are monitoring
    {
        FD_ZERO(&openSockets);
        FD_SET(listenSock, &openSockets);
        maxfds = listenSock;
    }

    finished = false;

    while (!finished)
    {
        // Get modifiable copy of readSockets
        readSockets = exceptSockets = openSockets;
        memset(buffer, 0, sizeof(buffer));

        // Look at sockets and see which ones have something to be read()
        int n = select(maxfds + 1, &readSockets, NULL, &exceptSockets, NULL);

        if (n < 0)
        {
            perror("select failed - closing down\n");
            finished = true;
        }
        else
        {
            // First, accept  any new connections to the server on the listening socket
            if (FD_ISSET(listenSock, &readSockets))
            {
                struct sockaddr_in *incoming = (struct sockaddr_in *)&client;
                struct in_addr incomingAddr = incoming->sin_addr;
                clientSock = accept(listenSock, (struct sockaddr *)&client,
                                    &clientLen);
                printf("accept***\n");
                // Add new client to the list of open sockets
                FD_SET(clientSock, &openSockets);

                // And update the maximum file descriptor
                maxfds = std::max(maxfds, clientSock);

                // create a new client to store information.
                clients[clientSock] = new Client(clientSock, incoming->sin_port, inet_ntoa(incomingAddr));

                // Decrement the number of sockets waiting to be dealt with
                n--;

                printf("Client connected on server: %d\n", clientSock);

            }
            // Now check for commands from clients
            std::list<Client *> disconnectedClients;
            while (n-- > 0)
            {
                for (auto const &pair : clients)
                {
                    Client *client = pair.second;

                    if (FD_ISSET(client->sock, &readSockets))
                    {
                        // recv() == 0 means client has closed connection
                        if (recv(client->sock, buffer, sizeof(buffer), MSG_DONTWAIT) == 0)
                        {
                            disconnectedClients.push_back(client);
                            // TODO: Fix this. Modifying the list as we work it is a nono
                            closeClient(client->sock, &openSockets, &maxfds);
                        }
                        // We don't check for -1 (nothing received) because select()
                        // only triggers if there is something on the socket for us.
                        else
                        {
                            std::cout << buffer << std::endl;
                            clientCommand(client->sock, &openSockets, &maxfds, buffer);
                        }
                    }
                }
                // Remove client from the clients list
                // TODO: When removing client, update maxfds, iterate through all of the lists and use max to set the number
                for (auto const &c : disconnectedClients)
                    clients.erase(c->sock);
            }
        }
    }
}
