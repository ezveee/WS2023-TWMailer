#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <iostream>

/*
   PROGRAM USAGE:
   server: ./bin/server <port> <any directory name for mailspool -> will be created if not exists>
   client: ./bin/client 127.0.0.1 <same port as the one used for server>
   
   seas
   there's not much change in this file compared to the ClientServerExample
   so if you don't wanna go over all of that, here's a list of the biggest changes:

   QUIT: line 125
   SEND: line 137
   LIST: line 181 (NOTE: messageNumber is based on the filename)
   READ: line 202
   DELETE: line 225

   all of these effectively do the same (so we won't comment each of them seperately):
      -> check which action (SEND, LIST, READ, DELETE) the user chose
      (strcasecmp with buffer and string literal)
      -> get user input for said action
      -> compile all of the inputs into one string
      -> send string to server via send() 

   main action happens in the server file (and both of the header files)
*/


///////////////////////////////////////////////////////////////////////////////

#define BUF 1024
#define PORT 6543

///////////////////////////////////////////////////////////////////////////////

int main(int argc, char **argv)
{
   int create_socket;
   char buffer[BUF];
   struct sockaddr_in address;
   int size;
   bool hasQuit = false;

   ////////////////////////////////////////////////////////////////////////////
   // CREATE A SOCKET
   if ((create_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
   {
      perror("Socket error");
      return EXIT_FAILURE;
   }

   ////////////////////////////////////////////////////////////////////////////
   // INIT ADDRESS   
   if (argc < 3)
   {
      fprintf(stderr, "Usage: %s <ip> <port>\n", argv[0]);
      return EXIT_FAILURE;
   }
   
   memset(&address, 0, sizeof(address));
   address.sin_family = AF_INET;
   address.sin_port = htons(atoi(argv[2]));

   inet_aton(argv[1], &address.sin_addr);

   ////////////////////////////////////////////////////////////////////////////
   // CREATE A CONNECTION
   if (connect(create_socket,
               (sockaddr*)&address,
               sizeof(address)) == -1)
   {
      perror("Connect error - no server available");
      return EXIT_FAILURE;
   }

   printf("Connection with server (%s) established\n",
          inet_ntoa(address.sin_addr));

   ////////////////////////////////////////////////////////////////////////////
   // RECEIVE DATA
   size = recv(create_socket, buffer, BUF - 1, 0);
   if (size == -1)
   {
      perror("recv error");
   }
   else if (size == 0)
   {
      printf("Server closed remote socket\n");
   }
   else
   {
      buffer[size] = '\0';
      printf("%s", buffer);
   }

   std::string sender, receiver, subject, message;
   std::string user, messageNumber;

   do
   {
      printf(">> ");
      if (fgets(buffer, BUF, stdin) != NULL)
      {
         int size = strlen(buffer);
         // remove new-line signs from string at the end
         if (buffer[size - 2] == '\r' && buffer[size - 1] == '\n')
         {
            size -= 2;
            buffer[size] = 0;
         }
         else if (buffer[size - 1] == '\n')
         {
            --size;
            buffer[size] = 0;
         }

         //////////////////////////////////////////////////////////////////////
         // QUIT
         if (strcasecmp(buffer, "QUIT") == 0)
         {
            hasQuit = true;
            if (send(create_socket, buffer, size, 0) == -1) 
            {
               perror("send error");
               break;
            }
         }

         //////////////////////////////////////////////////////////////////////
         // SEND
         else if (strcasecmp(buffer, "SEND") == 0)
         {
            std::cout << "Sender: ";
            do
            {
               std::getline(std::cin, sender);
            }
            while (sender.length() > 8 || sender.empty());

            std::cout << "Receiver: ";
            do
            {
               std::getline(std::cin, receiver);
            }
            while (receiver.length() > 8 || receiver.empty());
            
            std::cout << "Subject: ";
            do
            {
               std::getline(std::cin, subject);
            }
            while (subject.length() > 80 || subject.empty());

            std::cout << "Message: " << std::endl;
            std::string line;
            while (std::getline(std::cin, line))
            {
               if (line == ".")
                  break;
               message += line + "\n";
            }

            std::string sendRequest = "SEND\n" + sender + "\n" + receiver + "\n" + subject + "\n" + message + ".\n";

            // send data to server
            if (send(create_socket, sendRequest.c_str(), sendRequest.length(), 0) == -1) 
            {
               perror("send error");
               break;
            }
         }

         //////////////////////////////////////////////////////////////////////
         // LIST
         else if (strcasecmp(buffer, "LIST") == 0)
         {
            std::cout << "User: ";
            do
            {
               std::getline(std::cin, user);
            }
            while (user.length() > 8 || user.empty());

            std::string listRequest = "LIST\n" + user + "\n";

            // send data to server
            if (send(create_socket, listRequest.c_str(), listRequest.length(), 0) == -1) 
            {
               perror("send error");
               break;
            }
         }

         //////////////////////////////////////////////////////////////////////
         // READ
         else if (strcasecmp(buffer, "READ") == 0)
         {
            std::cout << "User: ";
            do
            {
               std::getline(std::cin, user);
            }
            while (user.length() > 8 || user.empty());
            std::cout << "Message number: ";
            std::getline(std::cin, messageNumber);

            std::string readRequest = "READ\n" + user + "\n" + messageNumber + "\n";

            // send data to server
            if (send(create_socket, readRequest.c_str(), readRequest.length(), 0) == -1) 
            {
               perror("send error");
               break;
            }
         }

         //////////////////////////////////////////////////////////////////////
         // DELETE
         else if (strcasecmp(buffer, "DEL") == 0)
         {
            std::cout << "User: ";
            do
            {
               std::getline(std::cin, user);
            }
            while (user.length() > 8 || user.empty());
            std::cout << "Message number: ";
            std::getline(std::cin, messageNumber);

            std::string deleteRequest = "DEL\n" + user + "\n" + messageNumber + "\n";

            // send data to server
            if (send(create_socket, deleteRequest.c_str(), deleteRequest.length(), 0) == -1) 
            {
               perror("send error");
               break;
            }
         }

         //////////////////////////////////////////////////////////////////////
         // MISCELLANEOUS INPUT
         else
            continue;

         //////////////////////////////////////////////////////////////////////
         // RECEIVE FEEDBACK
         size = recv(create_socket, buffer, BUF - 1, 0);
         if (size == -1)
         {
            perror("recv error");
            break;
         }
         else if (size == 0)
         {
            printf("Server closed remote socket\n");
            break;
         }
         else
         {
            buffer[size] = '\0';
            printf("<< %s", buffer);
         }
      }
   } while (!hasQuit);

   ////////////////////////////////////////////////////////////////////////////
   // CLOSES THE DESCRIPTOR
   if (create_socket != -1)
   {
      if (shutdown(create_socket, SHUT_RDWR) == -1)
      {
         perror("shutdown create_socket"); 
      }
      if (close(create_socket) == -1)
      {
         perror("close create_socket");
      }
      create_socket = -1;
   }

   return EXIT_SUCCESS;
}
