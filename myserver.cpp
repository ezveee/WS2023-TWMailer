#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>

// HEADER FILES
#include "requestHandling.h"

/*
   PROGRAM USAGE:
   server: ./bin/server <port> <any directory name for mailspool -> will be created if not exists>
   client: ./bin/client 127.0.0.1 <same port as the one used for server>
   
   functions other than the ones from the ClientServerSample were outsourced into seperate header files
   if a function from one of those is called, there'll be a comment in which one it is

   again, pretty much the same as the ClientServerSample
   biggest change is the ifs in the do while in clientCommunication() (line 198)
*/

// DEFINES
#define BUF 1024

// GLOBAL VARIABLES
int abortRequested = 0;
int create_socket = -1;
int new_socket = -1;

// FORWARD DECLARATIONS
void* clientCommunication(void* data);
void signalHandler(int sig);

// MAIN
int main(int argc, char **argv)
{
   socklen_t addrlen;
   struct sockaddr_in address, cliaddress;
   int reuseValue = 1;

   if (signal(SIGINT, signalHandler) == SIG_ERR)
   {
      perror("signal can not be registered");
      return EXIT_FAILURE;
   }

   if ((create_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
   {
      perror("Socket error"); // errno set by socket()
      return EXIT_FAILURE;
   }

   if (setsockopt(create_socket,
                  SOL_SOCKET,
                  SO_REUSEADDR,
                  &reuseValue,
                  sizeof(reuseValue)) == -1)
   {
      perror("set socket options - reuseAddr");
      return EXIT_FAILURE;
   }

   if (setsockopt(create_socket,
                  SOL_SOCKET,
                  SO_REUSEPORT,
                  &reuseValue,
                  sizeof(reuseValue)) == -1)
   {
      perror("set socket options - reusePort");
      return EXIT_FAILURE;
   }


   ////////////////////////////////////////////////////////////////////////////
   // INIT ADDRESS   
   if (argc < 3)
   {
      fprintf(stderr, "Usage: %s <port> <mail-spool-directoryname>\n", argv[0]);
      return EXIT_FAILURE;
   }
   
   memset(&address, 0, sizeof(address));
   address.sin_family = AF_INET;
   address.sin_addr.s_addr = INADDR_ANY;
   address.sin_port = htons(atoi(argv[1]));


   if (bind(create_socket, (struct sockaddr *)&address, sizeof(address)) == -1)
   {
      perror("bind error");
      return EXIT_FAILURE;
   }

   if (listen(create_socket, 5) == -1)
   {
      perror("listen error");
      return EXIT_FAILURE;
   }

   // open/create mail spool folder
   navigateToFolder(argv[2]);
   

   while (!abortRequested)
   {
      printf("Waiting for connections...\n");

      addrlen = sizeof(struct sockaddr_in);
      if ((new_socket = accept(create_socket,
                               (struct sockaddr *)&cliaddress,
                               &addrlen)) == -1)
      {
         if (abortRequested)
         {
            perror("accept error after aborted");
         }
         else
         {
            perror("accept error");
         }
         break;
      }

      printf("Client connected from %s:%d...\n",
             inet_ntoa(cliaddress.sin_addr),
             ntohs(cliaddress.sin_port));

      pthread_t clientThread;
      int* clientSocket = (int*)malloc(sizeof(int));
      *clientSocket = new_socket;

      if(pthread_create(&clientThread, NULL, &clientCommunication, (void*)clientSocket) != 0)
      {
         perror("error creating thread");
      }

      if(pthread_detach(clientThread) != 0)
      {
         perror("error detaching thread");
      }

      new_socket = -1;
   }


   // frees the descriptor
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

// FUNCTIONS
void* clientCommunication(void *data)
{
   char buffer[BUF];
   int size;
   int* current_socket = (int*)data;

   std::cout << "The thread " << pthread_self() << " has started communciating with the server." << std::endl;

   // pthread_mutex_lock(&mutex);
   strcpy(buffer, "Welcome to twmailer!\r\nPlease enter your commands...\r\n(LOGIN, SEND, READ, LIST, DEL, QUIT)\r\n");
   if (send(*current_socket, buffer, strlen(buffer), 0) == -1)
   {
      perror("send failed");
      return NULL;
   }

   do
   {
      size = recv(*current_socket, buffer, BUF - 1, 0);
      if (size == -1)
      {
         if (abortRequested)
         {
            perror("recv error after aborted");
         }
         else
         {
            perror("recv error");
         }
         break;
      }

      if (size == 0)
      {
         printf("Client closed remote socket\n"); // ignore error
         break;
      }

      // remove ugly debug message, because of the sent newline of client
      if (buffer[size - 2] == '\r' && buffer[size - 1] == '\n')
      {
         size -= 2;
      }
      else if (buffer[size - 1] == '\n')
      {
         --size;
      }

      buffer[size] = '\0';

      printf("Message received: %s\n", buffer);

      // turn buffer into std::string
      // create stringstream from said string
      // use getline to read first line of stream into action variable
      std::string request(buffer);
      std::string action;
      std::istringstream stream(request);
      std::getline(stream, action);
      
      // handle...Request(...) functions in requestHandling.h
      if (action == "LOGIN")
         handleLoginRequest(&stream, current_socket);

      else if (action == "SEND")
         handleSendRequest(&stream, current_socket);

      else if (action == "LIST")
         handleListRequest(&stream, current_socket);

      else if (action == "READ")
         handleReadRequest(&stream, current_socket);
         
      else if (action == "DEL")
         handleDeleteRequest(&stream, current_socket);
         
   } while (strcasecmp(buffer, "QUIT") != 0 && !abortRequested);

   //ldap_unbind_ext_s(ldapHandle, NULL, NULL);

   // closes/frees the descriptor if not already
   if (*current_socket != -1)
   {
      if (shutdown(*current_socket, SHUT_RDWR) == -1)
      {
         perror("shutdown new_socket");
      }
      if (close(*current_socket) == -1)
      {
         perror("close new_socket");
      }
      *current_socket = -1;
   }

   return NULL;
}

void signalHandler(int sig)
{
   if (sig == SIGINT)
   {
      printf("abort Requested... "); // ignore error
      abortRequested = 1;
      if (new_socket != -1)
      {
         if (shutdown(new_socket, SHUT_RDWR) == -1)
         {
            perror("shutdown new_socket");
         }
         if (close(new_socket) == -1)
         {
            perror("close new_socket");
         }
         new_socket = -1;
      }

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
   }
   else
   {
      exit(sig);
   }
}
