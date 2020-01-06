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

//struct for clients
struct client
{
    int score;
    int lives;
    int descriptor;
    int number;

    client()
    {
        score = 0;
        lives = 0;
    }
};

// server socket
int servFd;

//Word
std::string toFindedWord;
int letterInWord;

// data for poll
int LIVES = 2;
int START_GAME = 3;

int descrCapacity = 16;
int playersListCapacity = 16;

int descrCount = 1;
int descrCountWaiting = 1;

int amountOfPlayers = 0;

client theBestPlayer;

bool gameStarted = false;

pollfd *descr;
// pollfd *descrWaiting;
client *playersList;

// handles SIGINT
void ctrl_c(int);

std::string startGame();

void sendToClient(int fd, char *buffer, int indexPlayer);

void generateWord();

// sends data to clientFds excluding fd
void sendToAllBut(int fd, char *buffer, int count);

// converts cstring to port
uint16_t readPort(char *txt);

// sets SO_REUSEADDR
void setReuseAddr(int sock);

void eventOnServFd(int revents)
{
    // Wszystko co nie jest POLLIN na gnieździe nasłuchującym jest traktowane
    // jako błąd wyłączający aplikację
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

        int res = write(clientFd, "Hello", 5);
        std::cout << "Res: " << res << std::endl;
        descrCount++;

        printf("new connection from: %s:%hu (fd: %d)\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port), clientFd);
    }
}

void eventStart(int indexInDescr)
{
    auto clientFd = descr[indexInDescr].fd;
    auto revents = descr[indexInDescr].revents;

    if (revents & POLLIN)
    {
        char buffer[255];
        int count = read(clientFd, buffer, 255);
        if (count < 1)
            revents |= POLLERR;
    }

    if (revents & ~POLLIN)
    {
        printf("removing %d\n", clientFd);

        // remove from description of watched files for poll
        descr[indexInDescr] = descr[descrCount - 1];
        descrCount--;

        shutdown(clientFd, SHUT_RDWR);
        close(clientFd);
    }
}

void eventOnClientFd(int indexInDescr, int indexPlayer)
{
    auto clientFd = descr[indexInDescr].fd;
    auto revents = descr[indexInDescr].revents;

    if (revents & POLLIN)
    {
        char buffer[255];
        int count = read(clientFd, buffer, 255);
        if (count < 1)
            revents |= POLLERR;
        else
            sendToClient(clientFd, buffer, indexPlayer);
    }

    if (revents & ~POLLIN)
    {
        printf("removing %d\n", clientFd);

        // remove from description of watched files for poll
        descr[indexInDescr] = descr[descrCount - 1];
        descrCount--;

        shutdown(clientFd, SHUT_RDWR);
        close(clientFd);
    }
}

int main(int argc, char **argv)
{
    // get and validate port number
    if (argc != 2)
        error(1, 0, "Need 1 arg (port)");
    auto port = readPort(argv[1]);

    // create socket
    servFd = socket(AF_INET, SOCK_STREAM, 0);
    if (servFd == -1)
        error(1, errno, "socket failed");

    // graceful ctrl+c exit
    signal(SIGINT, ctrl_c);
    // prevent dead sockets from throwing pipe errors on write
    signal(SIGPIPE, SIG_IGN);

    setReuseAddr(servFd);

    // bind to any address and port provided in arguments
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
    bool enoughPlayers = false;
    bool stop = false;

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
                bool donePlayer = true;
                printf("Number of read %d \n", ready);
                printf("Descriptor number %d \n", descr[i].fd);

                if (descr[i].fd == servFd)
                {
                    printf(" 1. Add clients\n");
                    eventOnServFd(descr[i].revents);
                    continue;
                }

                for (int j = 0; j < playersListCapacity && gameStarted; j++)
                {
                    if (descr[i].fd == playersList[j].descriptor && playersList[j].lives <= LIVES)
                    {
                        printf(" 1. Event Player\n");
                        eventOnClientFd(i, j);

                        if (theBestPlayer.score < playersList[j].score)
                        {
                            theBestPlayer = playersList[j];
                        }
                        donePlayer = false;
                    }
                }

                if (donePlayer)
                {
                    printf(" 1. Event Start Works\n");
                    eventStart(i);
                }

                ready--;
            }

            if (amountOfPlayers <= 1 && gameStarted && !stop)
            {
                int i = 1;

                std::string startString;
                startString.append("Won the ");
                startString.append(std::to_string(theBestPlayer.number));
                startString.append(" Player");
                while (i < descrCount)
                {
                    int clientFd = descr[i].fd;
                    if (clientFd == theBestPlayer.descriptor)
                    {
                        write(clientFd, "You won", 7);
                        continue;
                    }
                    int res = write(clientFd, startString, startString.length());
                    if (res != startString.length())
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

                stop = true;
            }
        }

        if (descrCount > START_GAME && !gameStarted)
        {
            playersListCapacity = descrCount;
            free(playersList);
            playersList = (client *)malloc(sizeof(client) * playersListCapacity);

            gameStarted = true;

            for (int i = 0; i < playersListCapacity; i++)
            {
                playersList[i].descriptor = descr[i].fd;
                playersList[i].number = ++i;
                write(descr[i].fd, startGame().c_str(), startGame().length() + 1);
            }

            amountOfPlayers = playersListCapacity;

            generateWord();

            break;
        }
    }
}

uint16_t readPort(char *txt)
{
    char *ptr;
    auto port = strtol(txt, &ptr, 10);
    if (*ptr != 0 || port < 1 || (port > ((1 << 16) - 1)))
        error(1, 0, "illegal argument %s", txt);
    return port;
}

void setReuseAddr(int sock)
{
    const int one = 1;
    int res = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    if (res)
        error(1, errno, "setsockopt failed");
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

void sendToClient(int fd, char *buffer, int indexPlayer)
{
    int i = 1;
    std::string strBuffer(buffer);
    std::size_t found = strBuffer.find(';'); //end of msg length; ex 1 in "2;1;22*"
    int numberLetter = atoi(strBuffer.substr(0, found).c_str());
    std::string bufferSyntax = strBuffer.substr(found + 1);

    if (strBuffer.length() == numberLetter + 1)
    {
        write(fd, "Fail buffor", 11);
        return;
    }
    else if (bufferSyntax.substr(2, bufferSyntax.length() - 3).compare(toFindedWord) == 1)
    {
        //check compare
        playersList[indexPlayer].score = playersList[indexPlayer].score + 10;
        write(fd, "5;4;1*", 6);
        return;
    }
    else if (numberLetter == 5)
    {
        char letter = strBuffer[4];

        std::vector<size_t> positions; // holds all the positions that sub occurs within str

        for (int i = 0; i < toFindedWord.size(); i++)
        {
            if (toFindedWord[i] == letter)
            {
                positions.push_back(i);
            }
        }

        if (positions.size() != 0)
        {
            // string
            std::string startString;
            startString.append(";1;");

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

            write(fd, startStringNew.c_str(), startStringNew.length());

            return;
        }
        else
        {
            playersList[indexPlayer].lives++;

            if (playersList[indexPlayer] >= LIVES)
            {
                amountOfPlayers--;
                write(fd, "5;1;3*", 6);
                return;
            }

            write(fd, "4;3;*", 5);
            return;
        }
    }
    else
    {
        // write(fd, "5;4;0*", 6);
        playersList[indexPlayer].lives++;

        if (playersList[indexPlayer] >= LIVES)
        {
            amountOfPlayers--;
            write(fd, "5;1;3*", 6);
            return;
        }

        write(fd, "5;4;0*", 6);
        return;
    }
}

void generateWord()
{
    toFindedWord = "ananas";
    letterInWord = toFindedWord.length();

    std::string startString;
    startString.append(";2;");
    startString.append(std::to_string(letterInWord));
    startString.append("*");
    std::string startStringNew;
    startStringNew.append(std::to_string(startString.length()));
    startStringNew.append(startString);

    for (int i = 0; i < descrCount; i++)
    {
        playersList[i].descriptor = descr[i].fd;
        write(descr[i].fd, startStringNew.c_str(), startStringNew.length());
    }
}

std::string startGame()
{
    std::string startString;
    startString.append(";1;2");
    startString.append(std::to_string(LIVES));
    startString.append("*");
    std::string startStringNew;
    startStringNew.append(std::to_string(startString.length()));
    startStringNew.append(startString);

    return startStringNew;
}

void sendToAllBut(int fd, char *buffer, int count)
{
    // int i = 1;
    // while (i < descrCount)
    // {
    //     int clientFd = descr[i].fd;
    //     if (clientFd == fd)
    //     {
    //         i++;
    //         continue;
    //     }
    // int res = write(fd, buffer, count);
    // if (res != count)
    // {
    //     printf("removing %d\n", clientFd);
    //     shutdown(clientFd, SHUT_RDWR);
    //     close(clientFd);
    //     descr[i] = descr[descrCount - 1];
    //     descrCount--;
    //     // continue;
    // }
    //     i++;
    // }
}
