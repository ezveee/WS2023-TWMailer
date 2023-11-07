#include "helperMethods.h"

/*
   most of the magic (spaghetti :D) happens here
   
   all the handle... functions take the user input (that was sent to the server)
   in form of a stringstream and seperate it into all it's components
   (sender, recipient, subject, message, whatever)
   and then use those components for different functionalities
*/

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
   navigateToFolder(receiver.c_str());

   // check if _index.txt exists
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
         ++fileCounter;

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
   listResponse.insert(0, std::to_string(fileCounter) + "\n");
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
}