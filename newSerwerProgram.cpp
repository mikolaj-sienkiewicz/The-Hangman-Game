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

//GLOBAL VARIABLES TO CHANGE
int LIVES = 2;
int MIN_PLAYERS_TO_START_GAME = 3;

//GLOBAL VARIABLES
//INT
int servFd;
int descrCapacity = 16;
int descrCount = 1;

int amountOfGamers = 0;
int amountOfAllPLayers = 0;

//BOOLEN
bool gameStarted = false;

//MALLOC
pollfd *descr;

std::vector<client> players;
//FUNCTIONS

void initFunction(int argc, char **argv);
void addUser(int revents);
void writeData(int fd, char *buffer, ssize_t count);
void ctrl_c(int);
void getMessageFromUser(int indexInDescr);

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
            if (descr[i].revents)
            {
                //ADD USER
                if (descr[i].fd == servFd)
                {
                    addUser(descr[i].revents);
                }
                else
                {
                    // getMessageFromUser(i);
                    for (int j = 0; j < players.size() && gameStarted; j++)
                    {
                        if (players[j].fd == descr[i].fd)
                        {
                            std::string gamer = "Players "+j+" \n";
                            writeData(players[j].fd, gamer, gamer.length());
                        }
                    }
                }
                ready--;

                // if (gameStarted)
                // {
                // }
            }
        }
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

        if (!gameStarted)
        {
            client newGamer;
            newGamer.fd = clientFd;

            players.push_back(newGamer);
            amountOfGamers++;
        }
        amountOfAllPLayers++;
        descrCount++;

        printf("new connection from: %s:%hu (fd: %d)\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port), clientFd);

        if (amountOfGamers >= MIN_PLAYERS_TO_START_GAME && !gameStarted)
        {
            amountOfAllPLayers = amountOfGamers;
            gameStarted = true;
        }
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

void getMessageFromUser(int indexInDescr)
{
    auto clientFd = descr[indexInDescr].fd;
    auto revents = descr[indexInDescr].revents;

    if (revents & POLLIN)
    {
        char buffer[255];
        int count = read(clientFd, buffer, 255);
        if (count < 1)
        {
            revents |= POLLERR;
        }

        // else
        //     sendToAllBut(clientFd, buffer, count);
    }

    if (revents & ~POLLIN)
    {
        printf("removing %d\n", clientFd);

        // remove from description of watched files for poll
        descr[indexInDescr] = descr[descrCount - 1];
        amountOfGamers--;
        amountOfAllPLayers--;
        descrCount--;

        shutdown(clientFd, SHUT_RDWR);
        close(clientFd);
    }
}
