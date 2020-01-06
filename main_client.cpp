#include <cstdlib>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <error.h>
#include <netdb.h>
#include <poll.h>
#include <sstream>
#include <iostream>
#include <string>
#include <cstring>
#include <regex>
#include <cstdlib>
#include <fstream>




//wiedza o grze od serwera
std::string GAMESTATUS ="1",LIVES="10",TOPSCORE="UNKNOWN",nPLAYERS="UNKNOWN",nSPECTATORS="UNKNOWN",POINTS="0",WORD="", WORDLENGTH="0";



struct decodedMessage
{
	std::string commandLength;
	std::string commandType;
	std::string commandMessage;
};


void writeData(int fd, char* buffer, ssize_t count) {
	auto ret = write(fd, buffer, count);
	if (ret == -1) error(1, errno, "write failed on descriptor %d", fd);
	if (ret != count) error(0, errno, "wrote less than requested to descriptor %d (%ld/%ld)", fd, count, ret);
}

void updateInitMonitor() {
	//Initialize stream string ate -> at the end append
	std::ostringstream oss(std::ostringstream::ate);
	// "\033[0;0f" -> ustawia kursor w lewym gornym rogu, "\033[2J" -> czysci ekran
	oss.str("\033[2J");
	oss << "\033[0;0f" << "############################ THE HANGMAN #############################\n" << "Waiting for players to join\nCurrently there are: "<<nSPECTATORS<<" players in the waiting room\n\n"<<"Press ENTER to refresh \nAnd remember that patience is key...\n";
	std::cout << oss.str();
}

void updateGameMonitor() {
	//Initialize stream string ate -> at the end append
	std::ostringstream oss(std::ostringstream::ate);
	// "\033[0;0f" -> ustawia kursor w lewym gornym rogu, "\033[2J" -> czysci ekran
	oss.str("\033[2J");
	oss << "\033[0;0f" << "############################ THE HANGMAN #############################\n"<<"Current Word: "<<WORD<<"\n" << "Points: " << POINTS << "\n" << "Lives: " << LIVES << "\n" << "Insert 1 letter to guess it, or more to go for the word : \n";
	std::cout << oss.str();
}

void updateWaitingMonitor() {
	//Initialize stream string ate -> at the end append
	std::ostringstream oss(std::ostringstream::ate);
	// "\033[0;0f" -> ustawia kursor w lewym gornym rogu, "\033[2J" -> czysci ekran
	oss.str("\033[2J");
	oss << "\033[0;0f" << "############################ THE HANGMAN #############################\n" << "Current Top Score: " << TOPSCORE << "\n" << "Number of players in the game: " << nPLAYERS << "\n" << "Number of spectators watching: " << nSPECTATORS << "\n" << "press ENTER to refresh the screen\n";
	std::cout << oss.str();
}

void encodeMessage(int sock, char * buffer, int received) {
	if (GAMESTATUS == "1") {
		//wysylam 1* po refresh encode()
		updateInitMonitor();

	}
	else if (GAMESTATUS == "2") {
		updateGameMonitor();
	}
	else {
		//wysylam 1* po refresh encode()
		updateWaitingMonitor();
	}
	//writeData(STDOUT_FILENO, "a", 1);
	//printf("a");
	writeData(sock, buffer, received);
}
decodedMessage decodeMessage(char* buffer, int received) {
	decodedMessage dM;
	//writeData(STDOUT_FILENO, "a", 1);
	std::string strBuffer(buffer);
	std::size_t found=strBuffer.find(';'); //end of msg length; ex 1 in "2;1;22*"
	std::size_t found2 = strBuffer.find(';',found+1); //end of msg type; ex 3 in "2;1;22*"

	//end of entire message in the buffor 
	std::size_t finish = strBuffer.find('*');

	dM.commandLength = strBuffer.substr(0, found);
	dM.commandType = strBuffer.substr(found + 1, found2 - found - 1);
	dM.commandMessage = strBuffer.substr(found2 + 1, finish - found2-1);
	std::string commandLength = strBuffer.substr(0, found);
	std::string commandType = strBuffer.substr(found + 1, found2 - found - 1);
	std::string commandMessage = strBuffer.substr(found2 + 1, finish - found2 - 1);
	
	
	/*if (commandType == "1") {

	}
	else if (commandType == "2") {
		//updateGameMonitor
	}*/

	/*convert string to char array and print the result
	char cstr[commandMessage.size() + 1];
	strcpy(cstr, &commandMessage[0]);
	writeData(STDOUT_FILENO, cstr, sizeof(cstr));*/

	

	//writeData(STDOUT_FILENO, buffer, received);
	return dM;
}

ssize_t readData(int fd, char* buffer, ssize_t buffsize) {
	auto ret = read(fd, buffer, buffsize);
	if (ret == -1) error(1, errno, "read failed on descriptor %d", fd);
	return ret;
}


int main(int argc, char** argv) {
	if (argc != 3) error(1, 0, "Need 2 args");

	addrinfo * resolved, hints = { .ai_flags = 0,.ai_family = AF_INET,.ai_socktype = SOCK_STREAM };
	int res = getaddrinfo(argv[1], argv[2], &hints, &resolved);
	if (res || !resolved) error(1, 0, "getaddrinfo: %s", gai_strerror(res));

	int sock = socket(resolved->ai_family, resolved->ai_socktype, 0);
	if (sock == -1) error(1, errno, "socket failed");

	res = connect(sock, resolved->ai_addr, resolved->ai_addrlen);
	if (res) error(1, errno, "connect failed");

	freeaddrinfo(resolved);

	// Lista deskryptorów do monitorowania
	pollfd desc[2];

	// Na pierwszej pozycji: standardowe wejście
	desc[0].fd = STDIN_FILENO;
	desc[0].events = POLLIN;

	// Na drugiej pozycji: standardowe wyjscie
	//desc[1].fd = STDOUT_FILENO;
	//desc[1].events = POLLOUT;

	// Na drugiej pozycji: gniazdo sieciowe
	desc[1].fd = sock;
	desc[1].events = POLLIN | POLLRDHUP;



	// W obu przypadkach poll ma oczekiwać na możliwość wykonania odczytu bez
	// blokowania, dla sock też na zamknięcie gniazda przez drugą stronę

	ssize_t received;
	char buffer[255];

	while (1) {
		// Wywołanie poll - czekanie na wystąpnienie zdarzenia (w tym 
		// przypadku możliwości odczytu danych bez blokowania)
		int ready = poll(desc, 3, -1);
		if (ready == -1) {
			shutdown(sock, SHUT_RDWR);
			close(sock);
			error(1, errno, "poll failed");
		}

		// w przypadku poll trzeba przejść przez całą przekazaną listę (lub do
		// znalezienia zdarzeń na tylu deskryptorach, ile zwrócił poll)
		for (int i = 0; i < 2; ++i) {
			// pole revents jest wypełniane przez poll opisem gotowych zdarzeń
			if (desc[i].revents & (POLLERR | POLLHUP | POLLRDHUP)) {
				// Jeśli poll raportuje błąd / EOF na pliku, zakończ program
				shutdown(sock, SHUT_RDWR);
				close(sock);
				exit(0);
			}
			if (desc[i].revents & POLLIN) {
				// jeśli program raportuje możliwość odczytu, odczytaj dane
				received = readData(desc[i].fd, buffer, 255);
				// jeśli przyszły z standardowego wejścia, zapisz do funkcji kodujacej i wysylajacej,
				// wpw wyslij do funkcji odczytujacej
				//auto whereToWrite = desc[i].fd == STDIN_FILENO ? sock : STDOUT_FILENO;

				//PART FOR SENDING
				if (desc[i].fd == STDIN_FILENO) {
					encodeMessage(sock, buffer, received);
				}
				//PART FOR RECEIVING 
				else {
					//wzorzecwiadomosci 
					//regex wzorzec( "2*" );

					decodedMessage receivedData=decodeMessage(buffer, received);
					/*convert string to char array and print the result
					char cstr[receivedData.commandLength.size() + 1];
					strcpy(cstr, &receivedData.commandLength[0]);
					writeData(STDOUT_FILENO, cstr, sizeof(cstr));*/
					//command regards game status
					if (receivedData.commandType == "1") {
						//inicjalizacja
						if (receivedData.commandMessage == "1") {
							GAMESTATUS = "1";
							updateInitMonitor();
						}
						//gra -> regex po 2 jest ilosc zyc
						//poczekalnia 
						else if (receivedData.commandMessage == "3") {
							GAMESTATUS = "3";
							updateWaitingMonitor();
						}
						else{
							//uzupelnia string WORD podkresleniami tyle ile jest po liczbie 2
							GAMESTATUS = "2";
							WORDLENGTH = receivedData.commandMessage.substr(1);
							char cstr[WORDLENGTH.size() + 1];
							strcpy(cstr, &WORDLENGTH[0]);
							int intWORDLENGTH = atol(cstr);
							for (int i = 0; i < intWORDLENGTH; i++)
							{
								WORD += "_";
							}
							updateGameMonitor();
						}
					}
				} 

				//writeData(whereToWrite, buffer, received);
			}

		}
	}
}
