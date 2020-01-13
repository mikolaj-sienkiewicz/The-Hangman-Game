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


//DEBUGGER GDB 
//dgb ./a.out y
//run runuje calosc (mozna dodac argumenty normalnie po) albo start zatryzmuje na pierwszej- uruchamianie
//break x.cpp:8 (zatrzymanie w linii 8)
//continue(uruchom dalej) albo step(wejdz glepbiej) albo next(przejdz do nastepnej instrukcji) albo untill
//bt mowi gdzie jestesmy w kodzie (backtrace)
//frame nr (patrzy z jakim argumentem funkcja na strosie z nr nr ma)
//print <buf> albo print strlen(buf) do podpatrywania co sie dzieje ze zmiennymi
// w proc/<pid>/fd wszystkie otwarte pliki

//wiedza o grze od serwera
std::string GAMESTATUS ="1",LIVES="10",TOPSCORE="UNKNOWN",nPLAYERS="UNKNOWN",nSPECTATORS="UNKNOWN",POINTS="0",WORD="", WORDLENGTH="0",MESSAGE_QUEUE="";
int intWORDLENGTH = 0;



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
	std::string theString = "";
	theString += std::string("\033[2J") + "\033[0;0f"+"############################ THE HANGMAN #############################\n" + "Waiting for players to join\nCurrently there are: " + nSPECTATORS + " players in the waiting room\n\n" + "Press ENTER to refresh \nAnd remember that patience is key...\n";
	printf ("%s",theString.c_str());
}

void updateGameMonitor() {
	std::string theString="";
	theString+= std::string("\033[2J") + "\033[0;0f"+"############################ THE HANGMAN #############################\n"+ "Current Word: " + WORD + "\n" + "Points: " + POINTS + "\n" + "Lives: " + LIVES + "\n" + "Insert 1 letter to guess it, or more to go for the word : \n";
	printf("%s",theString.c_str());
}

void updateWaitingMonitor() {
	std::string theString = "";
	theString += std::string("\033[2J") + "\033[0;0f" + "############################ THE HANGMAN #############################\n" + "Current Top Score: " +TOPSCORE + "\n" + "Number of players in the game: " + nPLAYERS + "\n" + "Number of spectators watching: " + nSPECTATORS + "\n" + "press ENTER to refresh the screen\n";
	printf("%s",theString.c_str());
}

void encodeMessage(int sock, char * buffer, int received) {

	//-1 zeby uciac \n na koncu
	std::string finalBuffer2(buffer, received-1);

	//TYLE ILE RECEIVED ENKODOWAC
	/*char subbuf[received]; //I TO SAMO CO NIZEJ 
	
	for (int i = 0; i < received-1; i++)
	{
		subbuf[i] = buffer[i];
	}
	std::string strBuffer(subbuf); //buffer[0;received]
	std::string finalBuffer = "";

	for (int i = 0; i < received-1; i++)
	{
		finalBuffer += strBuffer[i];
	}*/

	//finalBuffer przechowuje obciety buffer po received

	if (GAMESTATUS == "1") {
		//wysylam 1* po refresh encode()
		updateInitMonitor();

	}
	else if (GAMESTATUS == "2") {

		std::string messageWithLength;
		std::string message;
		//std::string strBuffer(buffer);
		
		//FINAL BUFFER MA NA KONCU \n
		message.append(";1;");
		message.append(finalBuffer2);
		message.append("*");


		messageWithLength.append(std::to_string(message.length()));
		messageWithLength.append(message);


		///UWAGA
		writeData(sock, messageWithLength.data(), strlen(messageWithLength.c_str()));

		//chyba nie trzeba updatowac bo dostaniemy odpowiedz od serwera ktora to ztriggeruje
		//updateGameMonitor();
	}
	else {
		//wysylam 1* po refresh encode()
		updateWaitingMonitor();
	}

}

decodedMessage decodeMessage(std::string strBuffer, int received) {

	decodedMessage dM;

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
	//wasnt empty -> message was taken from queue not it needs to be deleted from it
	if (MESSAGE_QUEUE != "") {
		
		//ucina od poprzedniej gwiazdki do konca (SPRAWDZONE)
		MESSAGE_QUEUE = MESSAGE_QUEUE.substr(finish + 1);


	}
	//was empty but buffer got more
	else if(finish != strBuffer.length()) {
		
		//wyglada jakby mogl byc memory leak
		MESSAGE_QUEUE += strBuffer.substr(finish + 1, strBuffer.length());

	}
	return dM;
}

ssize_t readData(int fd, char* buffer, ssize_t buffsize) {
	auto ret = read(fd, buffer, buffsize);
	if (ret == -1) error(1, errno, "read failed on descriptor %d", fd);
	return ret;
}

int main(int argc, char** argv) {
	if (argc != 3) error(1, 0, "Need 2 args");

	setbuf(stdout, NULL);
	
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
		int ready=0;

		if (MESSAGE_QUEUE == "")
		{
			ready = poll(desc, 2, -1);
		}
		
		// nie zainicjonowane
		if (ready == -1) {
			shutdown(sock, SHUT_RDWR);
			close(sock);
			error(1, errno, "poll failed");
		}


	///CZYSCIC POLL.REEVENTS JAK JUZ WEJDZIESZ W IFA 
	//POLL WYPELNIL STRUKTURE A POTEM JAKBY ZOSTANIE TA SAMA WARTOSC WIEC REEVENTS TRZEBA CZYSCIC
	//if ready!=0 ALE WTEDY ZOBACZYC CZY IF Z MESSAGE QUEUE NIE TRZEBA WYWALIC ZA FOR
		for (int i = 0; i < 2; ++i) {
			// pole revents jest wypełniane przez poll opisem gotowych zdarzeń
			if (desc[i].revents & (POLLERR | POLLHUP | POLLRDHUP)) {
				// Jeśli poll raportuje błąd / EOF na pliku, zakończ program
				shutdown(sock, SHUT_RDWR);
				close(sock);
				exit(0);
			}
			if (desc[i].revents & POLLIN) {
				
				//czyszczenie revents, poniewaz w przeciwnym wypadku jak message queue nie bedzie puste to poll nie wyczysci tych pól
				desc[i].revents=0;

				//std::cout << "MESSAGE_QUEUE FROM THE INSIDE: " << MESSAGE_QUEUE << "\n";
				// jeśli program raportuje możliwość odczytu, odczytaj dane
				received = readData(desc[i].fd, buffer, 255);
				std::string finalBuffer3(buffer,received);
				std::cout<<finalBuffer3;

				//PART FOR SENDING
				if (desc[i].fd == STDIN_FILENO) {
					encodeMessage(sock, buffer, received);
				}
				//PART FOR RECEIVING 
				else {
					///TO SIE ZAPELNIA BO SUBBUFER UCINA BUFER ALE PRZEZ TO JEST WYCIEK tworzyc stringa konstruktorem przyjmujacy poczatek tekstu i dlugosc  std::string strName(buf,30) buf to pocztek bufora a 30 to received
					std::string finalBuffer2(buffer,received);
					MESSAGE_QUEUE += finalBuffer2;
				}
			}
		}
		if (MESSAGE_QUEUE != "") {
				decodedMessage receivedData = decodeMessage(MESSAGE_QUEUE, received);
				
				std::string debugString = "" + receivedData.commandLength + ";" + receivedData.commandType + ";" + receivedData.commandMessage + "*";

				//response regards game status ex 5;1;210 - go to game status and set lives to 10
				if (receivedData.commandType == "1") {
					//inicjalizacja
					if (receivedData.commandMessage == "1") {
						GAMESTATUS = "1";
						updateInitMonitor();
					}
					//poczekalnia 
					else if (receivedData.commandMessage == "3") {
						GAMESTATUS = "3";
						updateWaitingMonitor();
					}
					else {
						//uzupelnia string WORD podkresleniami tyle ile jest po liczbie 2
						GAMESTATUS = "2";
						LIVES = receivedData.commandMessage.substr(1);
						//char cstr[WORDLENGTH.size() + 1];
						//strcpy(cstr, &WORDLENGTH[0]);
						//int intWORDLENGTH = atol(cstr);
						//for (int i = 0; i < intWORDLENGTH; i++)
						//{
						//	WORD += "_";
						//}
						updateGameMonitor();
					}
				}
				//response regards the number of letters in the new word ex 6;2;30* - 30 letters in the word
				else if (receivedData.commandType == "2") {
					WORDLENGTH = receivedData.commandMessage;
					char cstr[WORDLENGTH.size() + 1];
					strcpy(cstr, &WORDLENGTH[0]);
					intWORDLENGTH = atol(cstr);
					for (int i = 0; i < intWORDLENGTH; i++)
					{
						WORD += "_";
					}
					updateGameMonitor();
				}
				//response regards the correctness of chosen letter 
				else if (receivedData.commandType == "3") {
					if (receivedData.commandMessage == "") {
						char cstr[LIVES.size() + 1];
						strcpy(cstr, &LIVES[0]);
						int  tmp= atol(cstr);
						tmp -= 1;
						LIVES = std::to_string(tmp);
						//LIVES -= 1;
					}
					//get letter positions and update the word array
					else {
						int nothing;
					}
					updateGameMonitor();
				}
			}
	}
}
