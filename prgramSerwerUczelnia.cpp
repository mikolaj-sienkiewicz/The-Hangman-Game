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

//struct for clients
struct client
{
    int score;
    int lives;

    client()
    {
        score = 0;
        lives = 0;
    }
};

// server socket
int servFd;

// data for poll
int startGame = 3;
int descrCapacity = 16;
int descrCapacityWaiting = 16;
int descrCount = 1;
int descrCountWaiting = 1;
pollfd * descr;
pollfd * descrWaiting;
client * clientsList;

// handles SIGINT
void ctrl_c(int);

// sends data to clientFds excluding fd
void sendToAllBut(int fd, char * buffer, int count);

// converts cstring to port
uint16_t readPort(char * txt);

// sets SO_REUSEADDR
void setReuseAddr(int sock);

void addToWaitingList(int revents) {
    // Wszystko co nie jest POLLIN na gnieździe nasłuchującym jest traktowane
    // jako błąd wyłączający aplikację
    if(revents & ~POLLIN){
        error(0, errno, "Event %x on server socket", revents);
        ctrl_c(SIGINT);
    }
    
    if(revents & POLLIN){
        sockaddr_in clientAddr{};
        socklen_t clientAddrSize = sizeof(clientAddr);
        
        auto clientFd = accept(servFd, (sockaddr*) &clientAddr, &clientAddrSize);
        if(clientFd == -1) error(1, errno, "accept failed");
        
        if(descrCountWaiting == descrCapacityWaiting){
            // Skończyło się miejsce w descr - podwój pojemność
            descrCapacityWaiting<<=1;
            descrWaiting = (pollfd*) realloc(descrWaiting, sizeof(pollfd)*descrCapacityWaiting);
            // clientsList = (client*) realloc(clientsList, sizeof(client)*descrCapacityWaiting);
        }
        
        descrWaiting[descrCountWaiting].fd = clientFd;
        descrWaiting[descrCountWaiting].events = POLLIN|POLLRDHUP;
        descrCountWaiting++;
        
        printf("new connection from: %s:%hu (fd: %d)\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port), clientFd);
    }
}

void eventOnServFd(int revents) {
    // Wszystko co nie jest POLLIN na gnieździe nasłuchującym jest traktowane
    // jako błąd wyłączający aplikację
    if(revents & ~POLLIN){
        error(0, errno, "Event %x on server socket", revents);
        ctrl_c(SIGINT);
    }
    
    if(revents & POLLIN){
        sockaddr_in clientAddr{};
        socklen_t clientAddrSize = sizeof(clientAddr);
        
        auto clientFd = accept(servFd, (sockaddr*) &clientAddr, &clientAddrSize);
        if(clientFd == -1) error(1, errno, "accept failed");
        
        if(descrCount == descrCapacity){
            // Skończyło się miejsce w descr - podwój pojemność
            descrCapacity<<=1;
            descr = (pollfd*) realloc(descr, sizeof(pollfd)*descrCapacity);
            clientsList = (client*) realloc(clientsList, sizeof(client)*descrCapacity);
        }
        
        descr[descrCount].fd = clientFd;
        descr[descrCount].events = POLLIN|POLLRDHUP;
        descrCount++;
        
        printf("new connection from: %s:%hu (fd: %d)\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port), clientFd);
    }
}

void eventStart(int indexInDescr) {
    auto clientFd = descr[indexInDescr].fd;
    auto revents = descr[indexInDescr].revents;
    
    if(revents & POLLIN){
        char buffer[255];
        int count = read(clientFd, buffer, 255);
        if(count < 1)
            revents |= POLLERR;
        
    }
    
    if(revents & ~POLLIN){
        printf("removing %d\n", clientFd);
        
        // remove from description of watched files for poll
        descr[indexInDescr] = descr[descrCount-1];
        descrCount--;
        
        shutdown(clientFd, SHUT_RDWR);
        close(clientFd);
    }
}

void eventOnClientFd(int indexInDescr) {
    auto clientFd = descr[indexInDescr].fd;
    auto revents = descr[indexInDescr].revents;
    
    if(revents & POLLIN){
        char buffer[255];
        int count = read(clientFd, buffer, 255);
        if(count < 1)
            revents |= POLLERR;
        else
            sendToAllBut(clientFd, buffer, count);
    }
    
    if(revents & ~POLLIN){
        printf("removing %d\n", clientFd);
        
        // remove from description of watched files for poll
        descr[indexInDescr] = descr[descrCount-1];
        descrCount--;
        
        shutdown(clientFd, SHUT_RDWR);
        close(clientFd);
    }
}

int main(int argc, char ** argv){
    // get and validate port number
    if(argc != 2) error(1, 0, "Need 1 arg (port)");
    auto port = readPort(argv[1]);
    
    // create socket
    servFd = socket(AF_INET, SOCK_STREAM, 0);
    if(servFd == -1) error(1, errno, "socket failed");
    
    // graceful ctrl+c exit
    signal(SIGINT, ctrl_c);
    // prevent dead sockets from throwing pipe errors on write
    signal(SIGPIPE, SIG_IGN);
    
    setReuseAddr(servFd);
    
    // bind to any address and port provided in arguments
    sockaddr_in serverAddr{.sin_family=AF_INET, .sin_port=htons((short)port), .sin_addr={INADDR_ANY}};
    int res = bind(servFd, (sockaddr*) &serverAddr, sizeof(serverAddr));
    if(res) error(1, errno, "bind failed");
    
    // enter listening mode
    res = listen(servFd, 1);
    if(res) error(1, errno, "listen failed");

    descr = (pollfd*) malloc(sizeof(pollfd)*descrCapacity);
    clientsList = (client*) malloc(sizeof(client)*descrCapacity);
    
    descr[0].fd = servFd;
    descr[0].events = POLLIN;
    
    while(true){
        bool enoughPlayers = false;
        int ready = poll(descr, descrCount, -1);
        if(ready == -1){
            error(0, errno, "poll failed");
            ctrl_c(SIGINT);
        }

        if(enoughPlayers){
                break;
        }
        
        for(int i = 0 ; i < descrCount && ready > 0 ; ++i){
            if(descr[i].revents){
                if(descr[i].fd == servFd)
                    eventOnServFd(descr[i].revents);
                else
                    eventStart(i);
                ready--;
            }

            if(descrCount >= startGame){
                enoughPlayers = true;
                break;
            }
        }
        
    }


    descrWaiting = (pollfd*) malloc(sizeof(pollfd)*descrCapacity);

    while(true){
        bool enoughPlayers = false;
        int ready = poll(descr, descrCount, -1);
        if(ready == -1){
            error(0, errno, "poll failed");
            ctrl_c(SIGINT);
        }
        
        for(int i = 0 ; i < descrCount && ready > 0 ; ++i){
            if(descr[i].revents){
                if(descr[i].fd == servFd)
                {
                    addToWaitingList(descr[i].revents);
                }
                else
                    eventOnClientFd(i);
                ready--;
            }

            if(descrCount <= 2){
                enoughPlayers = true;
                break;
            }
        }
        if(enoughPlayers){
                break;
        }
    }
}

uint16_t readPort(char * txt){
    char * ptr;
    auto port = strtol(txt, &ptr, 10);
    if(*ptr!=0 || port<1 || (port>((1<<16)-1))) error(1,0,"illegal argument %s", txt);
    return port;
}

void setReuseAddr(int sock){
    const int one = 1;
    int res = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    if(res) error(1,errno, "setsockopt failed");
}

void ctrl_c(int){
    for(int i = 1 ; i < descrCount; ++i){
        shutdown(descr[i].fd, SHUT_RDWR);
        close(descr[i].fd);
    }
    close(servFd);
    printf("Closing server\n");
    exit(0);
}

void sendToAllBut(int fd, char * buffer, int count){
    int i = 1;
    while(i < descrCount){
        int clientFd = descr[i].fd;
        if(clientFd == fd) {
            i++;
            continue;
        }
        int res = write(clientFd, buffer, count);
        if(res!=count){
            printf("removing %d\n", clientFd);
            shutdown(clientFd, SHUT_RDWR);
            close(clientFd);
            descr[i] = descr[descrCount-1];
            descrCount--;
            continue;
        
        }
        i++;
    }
}
