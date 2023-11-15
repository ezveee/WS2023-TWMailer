#include <iostream>
#include <fstream>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/socket.h>
#include <filesystem>
#include <termios.h>
#include <stdio.h>
#include <ldap.h>

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

int getch()
{
    int ch;
    struct termios t_old, t_new;
    tcgetattr(STDIN_FILENO, &t_old);

    t_new = t_old;
    
    t_new.c_lflag &= ~(ICANON | ECHO);
    
    tcsetattr(STDIN_FILENO, TCSANOW, &t_new);

    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &t_old);

    return ch;
}

void getpass(char* inputPassword)
{
    int show_asterisk = 0;

    const char BACKSPACE = 127;
    const char RETURN = 10;

    unsigned char ch = 0;
    std::string password;

    printf("Password: ");

    while ((ch = getch()) != RETURN)
    {
        if (ch == BACKSPACE)
        {
            if (password.length() != 0)
            {
                if (show_asterisk)
                {
                    printf("\b \b"); // backslash: \b
                }
                password.resize(password.length() - 1);
            }
        }
        else
        {
            password += ch;
            if (show_asterisk)
            {
                printf("*");
            }
        }
    }
    printf("\n");
    strcpy(inputPassword, password.c_str());
}


LDAP* LDAPinit()
{
    ////////////////////////////////////////////////////////////////////////////
    // LDAP config
    // anonymous bind with user and pw empty
    const char *ldapUri = "ldap://ldap.technikum-wien.at:389";
    const int ldapVersion = LDAP_VERSION3;
    int rc = 0; // return code

    ////////////////////////////////////////////////////////////////////////////
    // setup LDAP connection
    LDAP* ldapHandle;
    rc = ldap_initialize(&ldapHandle, ldapUri);
    if (rc != LDAP_SUCCESS)
    {
        fprintf(stderr, "ldap_init failed\n");
        exit(1);
    }
    printf("connected to LDAP server %s\n", ldapUri);

    ////////////////////////////////////////////////////////////////////////////
    // set verison options
    rc = ldap_set_option(
        ldapHandle,
        LDAP_OPT_PROTOCOL_VERSION, // OPTION
        &ldapVersion);             // IN-Value
    if (rc != LDAP_OPT_SUCCESS)
    {
        fprintf(stderr, "ldap_set_option(PROTOCOL_VERSION): %s\n", ldap_err2string(rc));
        ldap_unbind_ext_s(ldapHandle, NULL, NULL);
        exit(1);
    }

    ////////////////////////////////////////////////////////////////////////////
    // start connection secure (initialize TLS)
    rc = ldap_start_tls_s(
        ldapHandle,
        NULL,
        NULL);
    if (rc != LDAP_SUCCESS)
    {
        fprintf(stderr, "ldap_start_tls_s(): %s\n", ldap_err2string(rc));
        ldap_unbind_ext_s(ldapHandle, NULL, NULL);
        exit(1);
    }

    return ldapHandle;
}