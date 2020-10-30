//
// Simple chat client for TSAM-409
//
// Command line: ./chat_client 4000 
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
#include <thread>

#include <iostream>
#include <sstream>
#include <thread>
#include <map>

// Threaded function for handling responss from server

std::string getCurrentTimestamp() { 
    std::time_t ts = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    // Convert timestamp to string, remove the \n that gets added on to the end
    return std::string(strtok(std::ctime(&ts), "\n")) + " ";
}


void listenServer(int serverSocket)
{
    int nread;                                  // Bytes read from socket
    char buffer[1025];                          // Buffer for reading input

    while(true)
    {
       memset(buffer, 0, sizeof(buffer));
       nread = read(serverSocket, buffer, sizeof(buffer));

       if(nread == 0)                      // Server has dropped us
       {
          printf("Over and Out\n");
          exit(0);
       }
       else if(nread > 0)
       {
          std::string tss = getCurrentTimestamp() + buffer + "\n";
          printf(tss.c_str());
       }
    }
}


int main(int argc, char* argv[])
{
   struct addrinfo hints, *svr;              // Network host entry for server
   struct sockaddr_in serv_addr;           // Socket address for server
   int serverSocket;                         // Socket used for server 
   int nwrite;                               // No. bytes written to server
   char buffer[1025];                        // buffer for writing to server
   bool finished;                   
   int set = 1;                              // Toggle for setsockopt
   std::string group = "P3_Group_82";

   if(argc != 3)
   {
        printf("Usage: ./client <ip number> <ip  port>\n");
        printf("Ctrl-C to terminate\n");
        exit(0);
   }

   hints.ai_family   = AF_INET;            // IPv4 only addresses
   hints.ai_socktype = SOCK_STREAM;

   memset(&hints,   0, sizeof(hints));

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
       printf("Failed to set SO_REUSEADDR for port %s\n", argv[2]);
       perror("setsockopt failed: ");
   }

   
   if(connect(serverSocket, (struct sockaddr *)&serv_addr, sizeof(serv_addr) )< 0)
   {
       if(errno != EINPROGRESS)
       {
         printf("Failed to open socket to server: %s\n", argv[1]);
         perror("Connect failed: ");
         exit(0);
       }
   }

    // Listen and print replies from server
   std::thread serverThread(listenServer, serverSocket);

   finished = false;
   while(!finished)
   {
       bzero(buffer, sizeof(buffer));

       fgets(buffer, sizeof(buffer), stdin);

       std::string buf(buffer);

       std::string token;
       std::vector<std::string> tokens;

        std::replace(buf.begin(), buf.end(), ',', ' ');
        
        // No new line from hitting enter please. Does mean we miss newlines in messages
        buf.erase(std::remove(buf.begin(), buf.end(), '\n'), buf.end());

        // Parse
        std::stringstream stream(buf);

        while (stream >> token) {
            tokens.push_back(token);
        }

       if (tokens[0].compare("GETMSG") == 0) {
           if (tokens.size() == 2) {
            std::string tss = getCurrentTimestamp() + buffer + "\n";
            printf(tss.c_str());
            std::string modifiedMessage = "*GET_MSG," + tokens[1] + "#";
            nwrite = send(serverSocket, modifiedMessage.c_str(), modifiedMessage.length(), 0);
           }
       }

       else if (tokens[0].compare("SENDMSG") == 0) {
           if (tokens.size() >= 3) {
                std::string tss = getCurrentTimestamp() + "" + buffer + "\n";
                printf(tss.c_str());
                std::string msg;
                for (auto i = tokens.begin() + 2; i != tokens.end(); i++)
                {
                    msg += *i + " ";
                }
                std::string modifiedMessage = "*SEND_MSG," + tokens[1] + "," + group + "," + msg + "#";
                nwrite = send(serverSocket, modifiedMessage.c_str(), modifiedMessage.length(), 0);
           } 
       }
    
       else {

           nwrite = send(serverSocket, buf.c_str(), strlen(buf.c_str()),0);
       }

       if(nwrite  == -1)
       {
           perror("send() to server failed: ");
           finished = true;
       }

   }
}
