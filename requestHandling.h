#include "helperMethods.h"

#include <ldap.h>

/*
   most of the magic (spaghetti :D) happens here
   
   all the handle... functions take the user input (that was sent to the server)
   in form of a stringstream and seperate it into all it's components
   (sender, recipient, subject, message, whatever)
   and then use those components for different functionalities
*/

struct clientInformation
{
   int* clientSocket;
   struct sockaddr_in cliaddress;
};

struct blacklistItem
{
   char ldapUser[128];
   struct clientInformation client;
   int blacklistCounter;
};

void handleLoginRequest(std::istringstream* stream, clientInformation* client)
{
   std::string user, password;
   int* current_socket = (int*)client->clientSocket;

   // seperate user input into variables
   if (!(std::getline(*stream, user) &&
      std::getline(*stream, password)))
   {
      std::cerr << "getline() error" << std::endl;
   }
   
   // TODO: do all the LDAP stuff here
   LDAP* ldapHandle = LDAPinit();

   // read username (bash: export ldapuser=<yourUsername>)
   char ldapBindUser[256];
   char rawLdapUser[128];
   
   strcpy(rawLdapUser, user.c_str());
   sprintf(ldapBindUser, "uid=%s,ou=people,dc=technikum-wien,dc=at", rawLdapUser);
   // std::cout << ldapBindUser << "\n" << rawLdapUser << std::endl;

   // read password (bash: export ldappw=<yourPW>)
   char ldapBindPassword[256];
   strcpy(ldapBindPassword, password.c_str());

   // general
   int rc = 0; // return code

   

   ////////////////////////////////////////////////////////////////////////////
   // bind credentials
   BerValue bindCredentials;
   bindCredentials.bv_val = (char *)ldapBindPassword;
   bindCredentials.bv_len = strlen(ldapBindPassword);
   BerValue *servercredp; // server's credentials
   rc = ldap_sasl_bind_s(
       ldapHandle,
       ldapBindUser,
       LDAP_SASL_SIMPLE,
       &bindCredentials,
       NULL,
       NULL,
       &servercredp);
   if (rc != LDAP_SUCCESS)
   {
      fprintf(stderr, "LDAP bind error: %s\n", ldap_err2string(rc));
      ldap_unbind_ext_s(ldapHandle, NULL, NULL);

      if (send(*current_socket, "Invalid credentials.", 25, 0) == -1)
      {
         perror("send answer failed");
      }
      return;
   }

   ldap_unbind_ext_s(ldapHandle, NULL, NULL);

   if (send(*current_socket, rawLdapUser, strlen(rawLdapUser) + 1, 0) == -1)
   {
      perror("send answer failed");
   }
   
}


void handleSendRequest(std::istringstream* stream, int* current_socket)
{
   std::string sender, receiver, subject, message, line;

   // seperate user input into variables
   if (std::getline(*stream, sender) &&
      std::getline(*stream, receiver) &&
      std::getline(*stream, subject))
   {
      message = "";
      while (std::getline(*stream, line) && line != ".")
      {
         message += line + "\n";
      }
   }
   else
      std::cerr << "getline() error" << std::endl;

   // helperMethods.h
   // change current working directory to recipient's folder
   pthread_mutex_lock(&mutex);
   navigateToFolder(receiver.c_str());
   pthread_mutex_unlock(&mutex);

   // check if _index.txt exists
   pthread_mutex_lock(&mutex); // lock the access to the index file (if two users access at same time two index files might be created)
   std::ifstream indexFile("_index.txt");
   if (!indexFile.is_open())
   {
      // if not -> create; write "0" into it
      std::ofstream newIndexFile("_index.txt");
      if (newIndexFile.is_open())
      {
         newIndexFile << "0";
         newIndexFile.close();
         std::cout << "Created and initialized _index.txt with '0'." << std::endl;
      }
      else
      {
         std::cerr << "Unable to create _index.txt." << std::endl;
      }
   }
   else
   {
      std::cout << "_index.txt already exists." << std::endl;
      indexFile.close();
   }
   pthread_mutex_unlock(&mutex);

   // helperMethods.h
   // creates new mail in recipients folder and sends response to client
   saveNewMail(&sender, &receiver, &subject, &message, current_socket);

   if (chdir("..") == 0)
   {
      std::cout << "Returned back to spool folder." << std::endl;
   }
   else
   {
      std::cerr << "Could not return back to spool folder." << std::endl;
   }
   
}

void handleListRequest(std::istringstream* stream, int* current_socket)
{
   // read user input into variable
   std::string excludeFile = "_index.txt";
   std::string listResponse, user;
   getline(*stream, user);

   fs::path currentPath = fs::current_path();
   fs::path directoryPath = currentPath / user;

   // check if requested user's inbox exists
   // if not -> send error message to client and return
   if (!(fs::exists(directoryPath) && fs::is_directory(directoryPath)))
   {
      std::cout << "The directory doesn't exist." << std::endl;
      if (send(*current_socket, "0\n", 2, 0) == -1)
      {
         perror("send answer failed");
      }
      return;
   }

   int fileCounter = 0;
   std::string filename;

   // iterate through every file in directory
   for (auto entry : fs::directory_iterator(directoryPath))
   {
      if (entry.is_regular_file() && entry.path().filename() != excludeFile)
      {
         // count each file that isn't "_index.txt"
         pthread_mutex_lock(&mutex);
         ++fileCounter;
         pthread_mutex_unlock(&mutex);

         // get filename without the .txt
         filename = entry.path().filename();
         size_t found = filename.rfind(".txt");
         if (found != std::string::npos)
            filename.erase(found, 4);

         std::ifstream file(entry.path());
         std::string line;
         for (int i = 0; i < 3 && std::getline(file, line); ++i)
         {
            // save third line (subject) of each message into listResponse
            if (i == 2)
               listResponse += filename + ": " + line + "\n";
         }
         file.close();
      }
   }

   // add fileCounter at start of string
   // then send list of all subjects back to client
   pthread_mutex_lock(&mutex);
   listResponse.insert(0, std::to_string(fileCounter) + "\n");
   pthread_mutex_unlock(&mutex);

   if (send(*current_socket, listResponse.c_str(), listResponse.length(), 0) == -1)
   {
      perror("send answer failed");
   }
}

void handleReadRequest(std::istringstream* stream, int* current_socket)
{
   // seperate user input into variables
   std::string readResponse, user, messageNumber;
   getline(*stream, user);
   getline(*stream, messageNumber);

   fs::path currentPath = fs::current_path();
   fs::path directoryPath = currentPath / user;

   // check if requested user's inbox exists
   // if not -> send error message to client and return
   if (!(fs::exists(directoryPath) && fs::is_directory(directoryPath)))
   {
      std::cout << "The directory/user doesn't exist." << std::endl;
      if (send(*current_socket, "ERR\n", 5, 0) == -1)
      {
         perror("send answer failed");
      }
      return;
   }

   bool fileFound = false;

   // iterate through every file in directory
   for (const auto& entry : fs::directory_iterator(directoryPath))
   {
      // if file is the one we're looking for
      if (entry.is_regular_file() && entry.path().filename() == messageNumber + ".txt")
      {
         std::ifstream file(entry.path());
         
         // add each line from file to readResponse
         std::string line;
         int lineCounter = 0;
         while (std::getline(file, line))
         {
            readResponse += (lineCounter == 0) ? "Sender: " : "";
            readResponse += (lineCounter == 1) ? "Receiver: " : "";
            readResponse += (lineCounter == 2) ? "Subject: " : "";
            readResponse += (lineCounter == 3) ? "Message: " : "";

            readResponse += line + "\n";
            ++lineCounter;
         }

         file.close();
         fileFound = true;
      }
   }

   // send error to client if file wasn't found
   if (!fileFound)
   {
      std::cout << "The file doesn't exist." << std::endl;
      if (send(*current_socket, "ERR\n", 5, 0) == -1)
      {
         perror("send answer failed");
      }
      return;
   }

   // otherwise send readResponse
   if (send(*current_socket, readResponse.c_str(), readResponse.length(), 0) == -1)
   {
      perror("send answer failed");
   }
}

void handleDeleteRequest(std::istringstream* stream, int* current_socket)
{
   // seperate user input into variables
   std::string username, messageNumber;

   if (!std::getline(*stream, username) || !std::getline(*stream, messageNumber))
   {
      std::cerr << "Failed to read username or message number" << std::endl;
      return;
   }

   // check if user's inboc exists
   fs::path userDirectory = username;
   if (!fs::is_directory(userDirectory))
   {
      std::cerr << "User directory does not exist: " << userDirectory << std::endl;
      if (send(*current_socket, "ERR\n", 5, 0) == -1)
      {
         perror("send answer failed");
      }
      return;
   }

   // check if message file exists
   std::string filename = messageNumber + ".txt";
   fs::path messageFile = userDirectory / filename;

   pthread_mutex_lock(&mutex);
   if (fs::exists(messageFile) && fs::is_regular_file(messageFile))
   {
      // if so -> delete
      try
      {
         fs::remove(messageFile);
         std::cout << "File deleted :D" << std::endl;
         if (send(*current_socket, "OK\n", 5, 0) == -1)
         {
               perror("send answer failed");
         }
      }
      catch (const std::exception& e)
      {
         std::cerr << "Failed to delete file: " << e.what() << std::endl;
         if (send(*current_socket, "ERR\n", 5, 0) == -1)
         {
               perror("send answer failed");
         }
      }
   }
   else
   {
      // otherwise send error to client
      std::cerr << "File not found or couldn't be deleted." << std::endl;
      if (send(*current_socket, "ERR\n", 5, 0) == -1)
      {
         perror("send answer failed");
      }
   }
   pthread_mutex_unlock(&mutex);
}