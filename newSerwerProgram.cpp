#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <error.h>
#include <netdb.h>
#include <poll.h>
#include <unordered_set>
#include <signal.h>
#include <string.h>
#include <iostream>
#include <cstdlib>
#include <vector>
#include <stdlib.h>
#include <time.h>
#include <algorithm>
#include <string>

struct client
{
    int score;
    int lives;
    int fd;

    client()
    {
        score = 0;
        lives = 0;
    }
};
//INDEX LIST OF PLAYERS
std::vector<int> playerIdentityList;

//LIST OF STRINGS
std::string wordsList[10] = {"zielony", "polak", "matka", "ananas", "graf", "matematyka", "czerwony", "niebieski", "pomaranczowy", "lis"};

//STRING WORD
std::string roundsWord;
int sizeOfWord = 0;

//GLOBAL VARIABLES TO CHANGE
int LIVES = 3;
int MIN_PLAYERS_TO_START_GAME = 3;

//GLOBAL VARIABLES
//INT
int servFd;
int descrCapacity = 16;
int descrCount = 1;

int amountOfAllPLayers = 0;

int topScore = 0;
int topPlayer = 0;

//BOOLEN
bool gameStarted = false;
bool gameLoaded = false;

//MALLOC
pollfd *descr;

std::vector<client> players;
//FUNCTIONS
void startGame();
void finishGame();
void game(int cliendFd);
void joinToTheProgramForUser(int cliendFd);
void initFunction(int argc, char **argv);
void addUser(int revents);
void writeData(int fd, char *buffer, ssize_t count);
void ctrl_c(int);
void getMessageFromInit(int indexInDescr);
void getMessageFromQueue(int indexInDescr);
void sendToUser(int fd, char *buffer, int count, int indexInDescr);
void subGame(int fd, char *buffer, int indexPlayer);
void startRound();

ssize_t readData(int fd, char *buffer, ssize_t buffsize);
uint16_t readPort(char *txt);

int main(int argc, char **argv)
{
    initFunction(argc, argv);

    while (true)
    {
        int ready = poll(descr, descrCount, -1);
        if (ready == -1)
        {
            error(0, errno, "poll failed");
            ctrl_c(SIGINT);
        }

        for (int i = 0; i < descrCount && ready > 0; ++i)
        {
            bool playersHasQuestion = true;
            if (descr[i].revents)
            {
                //ADD USER
                if (descr[i].fd == servFd)
                {
                    addUser(descr[i].revents);
                    ready--;

                    joinToTheProgramForUser(descr[descrCount - 1].fd);
                    continue;
                }
                // getMessageFromInit(i);
                for (int j = 0; j < players.size() && gameStarted; j++)
                {
                    printf("PETLA FOR CONTROLLER: %d\n", descr[i].fd);
                    if (players[j].fd == descr[i].fd)
                    {
                        for (auto &existPlayer : playerIdentityList)
                        {
                            if (existPlayer == players[j].fd)
                            {
                                if (topScore < players[j].score)
                                {
                                    topScore = players[j].score;
                                    topPlayer = players[j].fd;
                                }
                                game(i);
                                playersHasQuestion = false;
                                ready--;
                                break;
                            }
                        }
                        printf("NOT EXIST %d", players[j].fd);
                        break;
                    }
                }

                finishGame();

                if (!gameStarted)
                {
                    printf("WITHOUT COMPATION: %d\n", descr[i].fd);
                    getMessageFromInit(i);
                    ready--;
                    continue;
                }
                if (gameStarted && playersHasQuestion)
                {
                    printf("IN QUE %d\n", descr[i].fd);
                    getMessageFromQueue(i);
                    ready--;
                    continue;
                }
            }
        }
    }
}

void joinToTheProgramForUser(int clientFd)
{
    if (gameStarted)
    {
        std::string codeMessage = ";7;" + std::to_string((amountOfAllPLayers - playerIdentityList.size())) + "-" + std::to_string(amountOfAllPLayers) + "-" + std::to_string(topScore) + "-" + std::to_string(topPlayer) + "*";
        std::string codeMessageFinal = std::to_string(codeMessage.length()) + codeMessage;
        writeData(clientFd, codeMessageFinal.data(), codeMessageFinal.length());
    }
    else
    {
        std::string codeMessage = ";6;" + std::to_string(amountOfAllPLayers) + "*";
        std::string codeMessageFinal = std::to_string(codeMessage.length()) + codeMessage;
        writeData(clientFd, codeMessageFinal.data(), codeMessageFinal.length());
    }
}

void initFunction(int argc, char **argv)
{
    srand(time(NULL));

    if (argc != 2)
        error(1, 0, "Need 1 arg (port)");
    auto port = readPort(argv[1]);

    servFd = socket(AF_INET, SOCK_STREAM, 0);
    if (servFd == -1)
        error(1, errno, "socket failed");

    // graceful ctrl+c exit
    signal(SIGINT, ctrl_c);
    // prevent dead sockets from throwing pipe errors on write
    signal(SIGPIPE, SIG_IGN);

    sockaddr_in serverAddr{.sin_family = AF_INET, .sin_port = htons((short)port), .sin_addr = {INADDR_ANY}};
    int res = bind(servFd, (sockaddr *)&serverAddr, sizeof(serverAddr));
    if (res)
        error(1, errno, "bind failed");

    // enter listening mode
    res = listen(servFd, 1);
    if (res)
        error(1, errno, "listen failed");

    descr = (pollfd *)malloc(sizeof(pollfd) * descrCapacity);
    descr[0].fd = servFd;
    descr[0].events = POLLIN;
}

void addUser(int revents)
{
    if (revents & ~POLLIN)
    {
        error(0, errno, "Event %x on server socket", revents);
        ctrl_c(SIGINT);
    }

    if (revents & POLLIN)
    {
        sockaddr_in clientAddr{};
        socklen_t clientAddrSize = sizeof(clientAddr);

        auto clientFd = accept(servFd, (sockaddr *)&clientAddr, &clientAddrSize);
        if (clientFd == -1)
            error(1, errno, "accept failed");

        if (descrCount == descrCapacity)
        {
            // Skończyło się miejsce w descr - podwój pojemność
            descrCapacity <<= 1;
            descr = (pollfd *)realloc(descr, sizeof(pollfd) * descrCapacity);
        }

        descr[descrCount].fd = clientFd;
        descr[descrCount].events = POLLIN | POLLRDHUP;

        std::string codeMessage = ";1;1-" + std::to_string(clientFd) + "*";
        std::string codeMessageFinal = std::to_string(codeMessage.length()) + codeMessage;

        writeData(clientFd, codeMessageFinal.data(), codeMessageFinal.length());

        amountOfAllPLayers++;
        descrCount++;

        printf("new connection from: %s:%hu (fd: %d)\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port), clientFd);

        startGame();
    }
}

void startGame()
{
    printf("HELLO:\n");
    if (amountOfAllPLayers >= MIN_PLAYERS_TO_START_GAME && !gameStarted)
    {
        amountOfAllPLayers;
        gameStarted = true;

        int i = 1;
        printf("TUTAJ JESTEM:\n");
        while (i < descrCount)
        {
            int clientFd = descr[i].fd;

            client newGamer;
            newGamer.fd = clientFd;

            players.push_back(newGamer);
            playerIdentityList.push_back(clientFd);
            std::string codeMessage = ";1;2-" + std::to_string(LIVES) + "*";
            std::string codeMessageFinal = std::to_string(codeMessage.length()) + codeMessage;
            printf("PETLA PETLA %d\n", clientFd);
            int res = write(clientFd, codeMessageFinal.data(), codeMessageFinal.length());

            printf("Number PETLA USED: %d\n", clientFd);
            if (res != codeMessageFinal.length())
            {
                printf("removing %d\n", clientFd);
                shutdown(clientFd, SHUT_RDWR);
                close(clientFd);
                descr[i] = descr[descrCount - 1];
                descrCount--;
                continue;
            }

            i++;
        }

        startRound();
    }
}

ssize_t readData(int fd, char *buffer, ssize_t buffsize)
{
    auto ret = read(fd, buffer, buffsize);
    if (ret == -1)
        error(1, errno, "read failed on descriptor %d", fd);
    return ret;
}

uint16_t readPort(char *txt)
{
    char *ptr;
    auto port = strtol(txt, &ptr, 10);
    if (*ptr != 0 || port < 1 || (port > ((1 << 16) - 1)))
        error(1, 0, "illegal argument %s", txt);
    return port;
}

void writeData(int fd, char *buffer, ssize_t count)
{
    auto ret = write(fd, buffer, count);
    if (ret == -1)
        error(1, errno, "write failed on descriptor %d", fd);
    if (ret != count)
        error(0, errno, "wrote less than requested to descriptor %d (%ld/%ld)", fd, count, ret);
}

void ctrl_c(int)
{
    for (int i = 1; i < descrCount; ++i)
    {
        shutdown(descr[i].fd, SHUT_RDWR);
        close(descr[i].fd);
    }
    close(servFd);
    printf("Closing server\n");
    exit(0);
}

void getMessageFromQueue(int indexInDescr)
{
    auto clientFd = descr[indexInDescr].fd;
    auto revents = descr[indexInDescr].revents;

    if (revents & POLLIN)
    {
        char buffer[255];
        int count = readData(clientFd, buffer, 255);
        if (count < 1)
        {
            revents |= POLLERR;
        }
        else
        {
            std::string codeMessage = ";7;" + std::to_string((amountOfAllPLayers - playerIdentityList.size())) + "-" + std::to_string(playerIdentityList.size()) + "-" + std::to_string(topScore) + "-" + std::to_string(topPlayer) + "*";
            std::string codeMessageFinal = std::to_string(codeMessage.length()) + codeMessage;
            writeData(clientFd, codeMessageFinal.data(), codeMessageFinal.length());
        }
    }

    if (revents & ~POLLIN)
    {
        printf("removing %d\n", clientFd);

        // remove from description of watched files for poll
        descr[indexInDescr] = descr[descrCount - 1];
        amountOfAllPLayers--;
        descrCount--;

        shutdown(clientFd, SHUT_RDWR);
        close(clientFd);
    }
}

void sendToUser(int fd, char *buffer, int count, int indexInDescr)
{
    int res = write(fd, buffer, count);

    if (res != count)
    {
        printf("removing %d\n", fd);
        shutdown(fd, SHUT_RDWR);
        close(fd);
        descr[indexInDescr] = descr[descrCount - 1];
        amountOfAllPLayers--;
        descrCount--;
    }
}

void getMessageFromInit(int indexInDescr)
{

    auto clientFd = descr[indexInDescr].fd;
    auto revents = descr[indexInDescr].revents;

    if (revents & POLLIN)
    {
        char buffer[255];
        int count = readData(clientFd, buffer, 255);
        if (count < 1)
        {
            revents |= POLLERR;
        }
        else
        {
            std::string codeMessage = ";6;" + std::to_string(amountOfAllPLayers) + "*";
            std::string codeMessageFinal = std::to_string(codeMessage.length()) + codeMessage;
            writeData(clientFd, codeMessageFinal.data(), codeMessageFinal.length());
        }
    }

    if (revents & ~POLLIN)
    {
        printf("removing %d\n", clientFd);

        // remove from description of watched files for poll
        descr[indexInDescr] = descr[descrCount - 1];
        amountOfAllPLayers--;
        descrCount--;

        shutdown(clientFd, SHUT_RDWR);
        close(clientFd);
    }
}

void game(int cliendFd)
{
    auto clientFd = descr[cliendFd].fd;
    auto revents = descr[cliendFd].revents;

    if (revents & POLLIN)
    {
        char buffer[255];
        int count = readData(clientFd, buffer, 255);
        if (count < 1)
            revents |= POLLERR;
        else
            subGame(clientFd, buffer, cliendFd);
    }

    if (revents & ~POLLIN)
    {
        playerIdentityList.erase(std::remove(playerIdentityList.begin(), playerIdentityList.end(), clientFd), playerIdentityList.end());

        printf("removing %d\n", clientFd);

        // remove from description of watched files for poll
        descr[cliendFd] = descr[descrCount - 1];
        amountOfAllPLayers--;
        descrCount--;

        shutdown(clientFd, SHUT_RDWR);
        close(clientFd);
    }
}

void startRound()
{
    roundsWord = wordsList[rand() % 10];
    sizeOfWord = roundsWord.length();

    printf("New word %s", roundsWord.c_str());

    std::string startString;
    startString.append(";2;");
    startString.append(std::to_string(sizeOfWord));
    startString.append("*");
    std::string startStringNew;
    startStringNew.append(std::to_string(startString.length()));
    startStringNew.append(startString);

    int i = 1;
    while (i < descrCount)
    {
        int clientFd = descr[i].fd;
        int res = write(clientFd, startStringNew.data(), startStringNew.length());
        if (res != startStringNew.length())
        {
            printf("removing %d\n", clientFd);
            shutdown(clientFd, SHUT_RDWR);
            close(clientFd);
            descr[i] = descr[descrCount - 1];
            descrCount--;
            continue;
        }

        i++;
    }
}

void finishGame()
{
    
    if (gameStarted && players.playerIdentityList() < 2)
    {
        printf("FINISHED\n");
        gameStarted = false;
        std::string startString;
        startString.append(";5;");
        startString.append(std::to_string(topScore)+"-"+std::to_string(topPlayer));
        startString.append("*");
        std::string startStringNew;
        startStringNew.append(std::to_string(startString.length()));
        startStringNew.append(startString);

        int i = 1;
        while (i < descrCount)
        {
            int clientFd = descr[i].fd;
            int res = write(clientFd, startStringNew.data(), startStringNew.length());
            if (res != startStringNew.length())
            {
                printf("removing %d\n", clientFd);
                shutdown(clientFd, SHUT_RDWR);
                close(clientFd);
                descr[i] = descr[descrCount - 1];
                descrCount--;
                continue;
            }
            i++;
        }
        printf("FINISHED\n");
        playerIdentityList.clear();
        startGame();
    }
}

void subGame(int fd, char *buffer, int indexPlayer)
{
    int i = 1;
    std::string strBuffer(buffer);
    std::size_t found = strBuffer.find_last_of(';'); //end of msg length; ex 1 in "2;1;22*"
    // int numberLetter = std::stoi(strBuffer.substr(0, found).data());
    int numberLetter;

    numberLetter = atoi(strBuffer.substr(0, found).c_str());
    // std::istringstream(strBuffer.substr(0, found)) >> numberLetter;
    // std::string bufferSyntax = strBuffer.substr(found + 1);

    printf("Send by client %d", numberLetter);
    std::string codeMessage = ";8;" + std::to_string((playerIdentityList.size())) + "*";
    std::string codeMessageFinal = std::to_string(codeMessage.length()) + codeMessage;
    writeData(fd, codeMessageFinal.data(), codeMessageFinal.length());
    // writeData(fd, codeMessageFinal, codeMessageFinal.length());

    // if (bufferSyntax.substr(2, bufferSyntax.length() - 3).compare(roundsWord) == 1)
    // {
    //     //check compare
    //     players[indexPlayer].score = players[indexPlayer].score + 10;

    //     printf("Finded word Player %d", indexPlayer);

    //     writeData(fd, "5;4;1*", 6);
    //     startRound();
    //     return;
    // }
    //else
    if (numberLetter == 5)
    {
        char letter = strBuffer[4];

        std::vector<size_t> positions; // holds all the positions that sub occurs within str

        for (int i = 0; i < roundsWord.size(); i++)
        {
            if (roundsWord[i] == letter)
            {
                positions.push_back(i);
            }
        }

        printf("Find Letter %c Player %d", letter, indexPlayer);

        if (positions.size() != 0)
        {
            // string
            std::string startString;
            startString.append(";3;");

            startString.append(std::to_string(positions[0]));

            for (int i = 1; i < positions.size(); i++)
            {
                startString.append("-");
                startString.append(std::to_string(positions[i]));
            }

            startString.append("*");
            std::string startStringNew;
            startStringNew.append(std::to_string(startString.length()));
            startStringNew.append(startString);

            write(fd, startStringNew.data(), startStringNew.length());

            return;
        }
        else
        {
            players[indexPlayer].lives++;

            if (players[indexPlayer].lives >= LIVES)
            {
                writeData(fd, "5;1;3*", 6);
                playerIdentityList.erase(std::remove(playerIdentityList.begin(), playerIdentityList.end(), fd), playerIdentityList.end());
                for (auto &i : playerIdentityList)
                {
                    std::cout << i << " ";
                }
                printf("\nDELETE PLAYER\n");

                return;
            }

            writeData(fd, "4;3;*", 5);
            return;
        }
    }
    else
    {
        // write(fd, "5;4;0*", 6);
        players[indexPlayer].lives++;

        if (players[indexPlayer].lives >= LIVES)
        {
            writeData(fd, "5;1;3*", 6);
            playerIdentityList.erase(std::remove(playerIdentityList.begin(), playerIdentityList.end(), fd), playerIdentityList.end());
            for (auto &i : playerIdentityList)
            {
                std::cout << i << " ";
            }
            printf("\nDELETE PLAYER\n");

            return;
        }

        write(fd, "5;4;0*", 6);
        return;
    }
}