#include <iostream>
#include <fstream>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/socket.h>
#include <filesystem>

namespace fs = std::filesystem;

/// @brief Checks if folder "folderName" exists: 
/// yes -> changes cwd;
/// no -> creates folder, then changes cwd
/// @param folderName 
void navigateToFolder(const char* folderName)
{
    fs::path directoryPath(folderName);

    if (fs::is_directory(directoryPath))
    {
        try
        {
            fs::current_path(directoryPath);
            std::cout << "Navigated to existing folder: " << folderName << std::endl;
        }
        catch (const fs::filesystem_error& ex)
        {
            std::cerr << "Failed to navigate to folder: " << folderName << ". Error: " << ex.what() << std::endl;
        }
    }
    else
    {
        try
        {
            fs::create_directory(directoryPath);
            fs::current_path(directoryPath);
            std::cout << "Created and navigated to new folder: " << folderName << std::endl;
        }
        catch (const fs::filesystem_error& ex)
        {
            std::cerr << "Failed to create or navigate to folder: " << folderName << ". Error: " << ex.what() << std::endl;
        }
    }
}

/// @brief Creates new message file in current folder.
/// @brief Bases name on index retrieved from "_index.txt"
/// @param sender 
/// @param receiver 
/// @param subject 
/// @param message 
/// @param current_socket 
void saveNewMail(std::string* sender, std::string* receiver, std::string* subject, std::string* message, int* current_socket)
{
    // read currentIndex from _index.txt file
    int currentIndex = 0;
    std::ifstream indexFile("_index.txt");
    if (indexFile.is_open())
    {
        indexFile >> currentIndex;
        indexFile.close();
    }
    else
    {
        std::cerr << "Error retrieving index" << std::endl;
    }

    // create new message file
    std::string filename = std::to_string(currentIndex) + ".txt";
    std::ofstream file(filename);

    if (file.is_open())
    {
        // write message data into file
        file << *sender << "\n";
        file << *receiver << "\n";
        file << *subject << "\n";
        file << *message;

        file.close();

        std::cout << "Data has been written to " << filename << std::endl;
        if (send(*current_socket, "OK\n", 5, 0) == -1)
        {
            perror("send answer failed");
        }

        // increment index in file 
        ++currentIndex;
        std::ofstream newIndexFile("_index.txt");
        if (newIndexFile.is_open())
        {
            newIndexFile << currentIndex;
            newIndexFile.close();
        }
        else
        {
            std::cerr << "Unable to update index." << std::endl;
        }
    }
    else
    {
        std::cerr << "Unable to open message file." << std::endl;
        if (send(*current_socket, "ERR\n", 5, 0) == -1)
        {
            perror("send answer failed");
        }
    }

}