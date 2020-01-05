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


void writeData(int fd, char* buffer, ssize_t count) {
	auto ret = write(fd, buffer, count);
	if (ret == -1) error(1, errno, "write failed on descriptor %d", fd);
	if (ret != count) error(0, errno, "wrote less than requested to descriptor %d (%ld/%ld)", fd, count, ret);
}


void updateGameMonitor(std::string points, std::string lives) {
	//Initialize stream string ate -> at the end append
	std::ostringstream oss(std::ostringstream::ate);
	// "\033[0;0f" -> ustawia kursor w lewym gornym rogu, "\033[2J" -> czysci ekran
	oss.str("\033[2J");
	oss << "\033[0;0f" << "############################ THE HANGMAN #############################\n" << "Points: " << points << "\n" << "Lives: " << lives << "\n" << "Insert 1 letter to guess it, or more to go for the word : \n";
	std::cout << oss.str();
}

void encodeMessage(int sock, char * buffer, int received) {
	printf("a");
	writeData(sock, buffer, received);
}
void decodeMessage(char* buffer, int received) {
	printf("b");
	writeData(STDOUT_FILENO, buffer, received);
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
	pollfd desc[3];

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
		for (int i = 0; i < 3; ++i) {
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
				desc[i].fd == STDIN_FILENO ? encodeMessage(sock,buffer,received) : decodeMessage(buffer,received);
				
				//writeData(whereToWrite, buffer, received);
			}
			if (desc[i].revents & POLLOUT) {

			}

		}
	}
}
