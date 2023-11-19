#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <vector>

#include "helperMethods.h"

/*
   PROGRAM USAGE:
   server: ./bin/server <port> <any directory name for mailspool -> will be created if not exists>
   client: ./bin/client 127.0.0.1 <same port as the one used for server>
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
   bool isLoggedIn = false;
   std::string sessionUser = "";

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
   int socketCopy = create_socket;

   size = recv(socketCopy, buffer, BUF - 1, 0);
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

   char ldapBindPassword[256];
   std::string LDAPuser, LDAPpassword;
   std::string sender, receiver, subject, message;
   std::string user, messageNumber;

   bool errorOccuredDuringLoop = false;
   bool endOfRead = false;

   // takes user input/commands and sends them to server
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
            // "client closed remote socket" message wouldn't show up while send was in 
            // if (send(create_socket, buffer, size, 0) == -1) 
            // {
            //    perror("send error");
            //    break;
            // }
            break;
         }
         
         //////////////////////////////////////////////////////////////////////
         // LOGIN
         else if (strcasecmp(buffer, "LOGIN") == 0)
         {
            // isLoggedIn set to false at start of program
            // -> changed upon login
            if (isLoggedIn)
            {
               std::cout << "<< You are already logged in. (" << sessionUser << ")\n";
               continue;
            }

            // gets user input until it is valid
            bool isValidUsername = false;
            while (!isValidUsername)
            {
               std::cout << "Username: ";
               std::getline(std::cin, LDAPuser);
               if (!(LDAPuser.length() > 8 || LDAPuser.empty()) && isStringValidInput(LDAPuser))
                  isValidUsername = true;
            }
            
            encrypt(42, LDAPuser);

            // getpass already encrypts password
            // getpass function from LDAP code sample
            // used to not show password when it's being typed in
            getpass(ldapBindPassword);

            std::string loginRequest = "LOGIN\n" + LDAPuser + "\n" + ldapBindPassword + "\n";

            // send data to server
            if (send(create_socket, loginRequest.c_str(), loginRequest.length() + 1, 0) == -1) 
            {
               perror("send error");
               break;
            }
         }

         //////////////////////////////////////////////////////////////////////
         // SEND
         else if (strcasecmp(buffer, "SEND") == 0)
         {
            // check if user is logged in
            // if not -> not permitted to use functions other than login/quit
            if (!isLoggedIn)
            {
               std::cout << "<< [PERMISSION DENIED]: To access this function, please log in.\n";
               continue;
            }

            // gets user input for recipient, subject and message
            bool isValidInput = false;
            while (!isValidInput)
            {
               std::cout << "Receiver: ";
               std::getline(std::cin, receiver);
               if (!(receiver.length() > 8 || receiver.empty()) && isStringValidInput(receiver))
                  isValidInput = true;
            }
            
            isValidInput = false;
            while (!isValidInput)
            {
               std::cout << "Subject: ";
               std::getline(std::cin, subject);
               if (!(subject.length() > 80 || subject.empty()))
                  isValidInput = true;
            }

            std::cout << "Message: " << std::endl;
            std::string line;

            while (std::getline(std::cin, line))
            {
               if (line == ".")
                  break;
               message += line + "\n";
            }

            long currentPositionInSendRequest = 0;
            long msgSize = ("SEND\n" + sessionUser + "\n" + receiver + "\n" + subject + "\n" + message + "\n.\n").length();

            std::string sendText = sessionUser + "\n" + receiver + "\n" + subject + "\n" + message + "\n.\n";
            std::string sendRequest;

            // send data to server
            do
            { //                                                                      BUF-6 <-"SEND\n" 
               sendRequest = "SEND\n" + sendText.substr(currentPositionInSendRequest,(BUF-6));
               currentPositionInSendRequest += BUF-6;
               if (send(create_socket, sendRequest.c_str(), sendRequest.length(), 0) == -1) 
               {
                  perror("send error");
                  errorOccuredDuringLoop = true;
                  break;
               }
            } 
            while (currentPositionInSendRequest < msgSize);
            
            if (errorOccuredDuringLoop) break;

            // clear message
            message = "";
         }

         //////////////////////////////////////////////////////////////////////
         // LIST
         else if (strcasecmp(buffer, "LIST") == 0)
         {
            // check if user is logged in
            // if not -> not permitted to use functions other than login/quit
            if (!isLoggedIn)
            {
               std::cout << "<< [PERMISSION DENIED]: To access this function, please log in.\n";
               continue;
            }

            std::string listRequest = "LIST\n" + sessionUser + "\n";

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
            // check if user is logged in
            // if not -> not permitted to use functions other than login/quit
            if (!isLoggedIn)
            {
               std::cout << "<< [PERMISSION DENIED]: To access this function, please log in.\n";
               continue;
            }

            // gets user input
            std::cout << "Message number: ";
            std::getline(std::cin, messageNumber);

            std::string readRequest = "READ\n" + sessionUser + "\n" + messageNumber + "\n";

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
            // check if user is logged in
            // if not -> not permitted to use functions other than login/quit
            if (!isLoggedIn)
            {
               std::cout << "<< [PERMISSION DENIED]: To access this function, please log in.\n";
               continue;
            }

            // gets user input
            std::cout << "Message number: ";
            std::getline(std::cin, messageNumber);

            std::string deleteRequest = "DEL\n" + sessionUser + "\n" + messageNumber + "\n";

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

         // checks if user used login command
         // if so -> recv from server gets handled differently 
         if (strcasecmp(buffer, "LOGIN") == 0)
         {
            size = recv(create_socket, buffer, BUF - 1, 0);
            if (size == -1)
            {
               perror("recv error");
               break;
            }
            if (size == 0)
            {
               printf("Server closed remote socket\n");
               break;
            }
            
            if ((strcmp(buffer, "Invalid credentials.") == 0))
            {
               buffer[size] = '\0';
               printf("<< %s\n", buffer);
               continue;
            }

            if ((strcmp(buffer, "You are timed out.") == 0))
            {
               buffer[size] = '\0';
               printf("<< %s\n", buffer);
               continue;
            }

            // valid credentials -> session user gets set
            isLoggedIn = true;
            sessionUser = buffer;
            std::cout << "<< Login was successful. Welcome " << sessionUser << "!\n";
            continue;

         }

         else if (strcasecmp(buffer, "READ") == 0)
         {
            do
            {
               size = recv(create_socket, buffer, BUF - 1, 0);
               if (size == -1)
               {
                  perror("recv error");
                  errorOccuredDuringLoop = true;
                  break;
               }
               else if (size == 0)
               {
                  printf("Server closed remote socket\n");
                  errorOccuredDuringLoop = true;
                  break;
               }
               else if(strcmp(buffer, "ERR\n") == 0)
               {
                  buffer[size] = '\0';
                  printf("<< %s", buffer);
                  endOfRead = true;
               }
               else
               {
                  // replace "\n." with '\0' because end of READ has been reached 
                  if(buffer[size-1] == '\n' && buffer[size-2] == '.' && buffer[size-3] == '\n')
                  {
                     size -= 3;
                     endOfRead = true;
                  } 
                  buffer[size] = '\0';
                  
                  printf("%s", buffer);
               }
            } 
            while (!endOfRead);

            endOfRead = false;

            if (errorOccuredDuringLoop) break;
            continue;
         }
         else
         {
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