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
#include <string>
#include <cstring>
#include <netinet/in.h>
#include <cctype>

namespace fs = std::filesystem;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; 

struct clientInformation
{
   int* clientSocket;
   struct sockaddr_in cliaddress;
};

struct blacklistItem
{
   std::string user;
   struct clientInformation client;
   int blacklistCounter = 0;
};

bool isThreadRunning = false;

// checks if user input is a valid String (no special characters)
bool isStringValidInput(std::string input)
{
    // check every char in input is alphanumeric
    for (char c : input)
        if (!std::isalnum(c))
            return false; 
    return true;
}

int getch()
{
    int ch;
    // https://man7.org/linux/man-pages/man3/termios.3.html
    struct termios t_old, t_new;

    // https://man7.org/linux/man-pages/man3/termios.3.html
    // tcgetattr() gets the parameters associated with the object referred
    //   by fd and stores them in the termios structure referenced by
    //   termios_p
    tcgetattr(STDIN_FILENO, &t_old);
    
    // copy old to new to have a base for setting c_lflags
    t_new = t_old;

    // https://man7.org/linux/man-pages/man3/termios.3.html
    //
    // ICANON Enable canonical mode (described below).
    //   * Input is made available line by line (max 4096 chars).
    //   * In noncanonical mode input is available immediately.
    //
    // ECHO   Echo input characters.
    t_new.c_lflag &= ~(ICANON | ECHO);
    
    // sets the attributes
    // TCSANOW: the change occurs immediately.
    tcsetattr(STDIN_FILENO, TCSANOW, &t_new);

    ch = getchar();

    // reset stored attributes
    tcsetattr(STDIN_FILENO, TCSANOW, &t_old);

    return ch;
}

/// @brief encrypts string msg with a ceaser cipher using ceaserShift (works with all askii-chars) and stores it given string
/// @param ceaserShift 
/// @param msg 
void encrypt(int ceaserShift, std::string& msg)
{  
    for (auto& c : msg) 
        c = (c + ceaserShift)%128;  
}

/// @brief decrypts string msg with a ceaser cipher using ceaserShift (works with all askii-chars) and stores it given string
/// @param ceaserShift 
/// @param msg 
void decrypt(int ceaserShift, std::string& msg)
{
    for (auto& c : msg)
    {
        if((ceaserShift-1) < c)
            c -= ceaserShift;
        else
            c = 128 - (ceaserShift - c);
    } 
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
    
    encrypt(42, password);
    strcpy(inputPassword, password.c_str());
}

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
    pthread_mutex_lock(&mutex);
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
    pthread_mutex_unlock(&mutex);

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
        pthread_mutex_lock(&mutex);
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
        pthread_mutex_unlock(&mutex);
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

void* timeout(void* data)
{
    blacklistItem* item = (blacklistItem*)data;

    std::cout << "The timeout thread " << pthread_self() << " has been started." << std::endl;

    sleep(60);

    std::cout << "The thread has awoken from it's peaceful slumber, man i wish that was my job, just sleeping any time i get a task" << std::endl;

    item->blacklistCounter = 0;
    isThreadRunning = false;

    return NULL;
}