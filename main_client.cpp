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
	fflush(stdout);
	fflush(stdin);
	//Initialize stream string ate -> at the end append
	//std::ostringstream oss(std::ostringstream::ate);
	// "\033[0;0f" -> ustawia kursor w lewym gornym rogu, "\033[2J" -> czysci ekran
	//oss.str("\033[2J");
	//oss << "\033[0;0f" << "############################ THE HANGMAN #############################\n" << "Waiting for players to join\nCurrently there are: "<<nSPECTATORS<<" players in the waiting room\n\n"<<"Press ENTER to refresh \nAnd remember that patience is key...\n";
	//std::cout << oss.str();
	std::string theString = "";
	//theString += std::string("\033[2J") + "\033[0;0f" + "############################ THE HANGMAN #############################\n" + "Waiting for players to join\nCurrently there are: " + nSPECTATORS + " players in the waiting room\n\n" + "Press ENTER to refresh \nAnd remember that patience is key...\n";
	//theString += std::string("\033[2J") + "\033[0;0f"+"############################ THE HANGMAN #############################\n" + "Waiting for players to join\nCurrently there are: " + nSPECTATORS + " players in the waiting room\n\n" + "Press ENTER to refresh \nAnd remember that patience is key...\n";
	theString += std::string("\033[2J") + "\033[0;0f" + "############################ THE HANGMAN #############################\n" + "Current Word: " + WORD + "\n" + "Points: " + POINTS + "\n" + "Lives: " + LIVES + "\n" + "Insert 1 letter to guess it, or more to go for the word : \n";

	char cstr[theString.size() + 1];
	strcpy(cstr, &theString[0]);

	printf(cstr);
	
	//writeData(STDOUT_FILENO, cstr, strlen(cstr));

}

void updateGameMonitor() {
	fflush(stdout);
	fflush(stdin);
	//Initialize stream string ate -> at the end append
	//std::ostringstream oss(std::ostringstream::ate);
	//oss.str("\033[2J");
	//oss << "\033[0;0f" << "############################ THE HANGMAN #############################\n"<<"Current Word: "<<WORD<<"\n" << "Points: " << POINTS << "\n" << "Lives: " << LIVES << "\n" << "Insert 1 letter to guess it, or more to go for the word : \n";
	//std::cout << oss.str();
	std::string theString2="";
	//theString+=std::string("\033[2J")+"\033[0;0f"+"############################ THE HANGMAN #############################\n" + "Current Word: " + WORD + "\n" + "Points: " + POINTS + "\n" + "Lives: " + LIVES + "\n" + "Insert 1 letter to guess it, or more to go for the word : \n";
	theString2+= std::string("\033[2J") + "\033[0;0f"+"############################ THE HANGMAN #############################\n"+ "Current Word: " + WORD + "\n" + "Points: " + POINTS + "\n" + "Lives: " + LIVES + "\n" + "Insert 1 letter to guess it, or more to go for the word : \n";

	char cstr2[theString2.size() + 1];
	strcpy(cstr2, &theString2[0]);
	//writeData(STDOUT_FILENO, "a", strlen("a"));
	printf(cstr2);
	
	
}

void updateWaitingMonitor() {
	//Initialize stream string ate -> at the end append
	//std::ostringstream oss(std::ostringstream::ate);
	// "\033[0;0f" -> ustawia kursor w lewym gornym rogu, "\033[2J" -> czysci ekran
	//oss.str("\033[2J");
	//oss << "\033[0;0f" << "############################ THE HANGMAN #############################\n" << "Current Top Score: " << TOPSCORE << "\n" << "Number of players in the game: " << nPLAYERS << "\n" << "Number of spectators watching: " << nSPECTATORS << "\n" << "press ENTER to refresh the screen\n";
	//std::cout << oss.str();

	std::string theString = "";
	theString += std::string("\033[2J") + "\033[0;0f" + "############################ THE HANGMAN #############################\n" + "Current Top Score: " +TOPSCORE + "\n" + "Number of players in the game: " + nPLAYERS + "\n" + "Number of spectators watching: " + nSPECTATORS + "\n" + "press ENTER to refresh the screen\n";
	char cstr[theString.size() + 1];
	strcpy(cstr, &theString[0]);
	printf(cstr);
	fflush(stdout);

	//writeData(STDOUT_FILENO, cstr, strlen(cstr));
}

void encodeMessage(int sock, char * buffer, int received) {

	//TYLE ILE RECEIVED ENKODOWAC
	char subbuf[received]; //I TO SAMO CO NIZEJ 
	//-1 zeby uciac \n na koncu
	for (int i = 0; i < received-1; i++)
	{
		subbuf[i] = buffer[i];
	}
	//std::cout << "subbuffer size : " << std::to_string(strlen(subbuffer)) << "\n";

	std::string strBuffer(subbuf); //buffer[0;received]
	std::string finalBuffer = "";

	for (int i = 0; i < received-1; i++)
	{
		finalBuffer += strBuffer[i];
	}

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
		message.append(finalBuffer);
		message.append("*");
		//std::cout<<"HERE: "  << finalBuffer << "\n";
		/*char cstr2[message.size() + 1];
		strcpy(cstr2, &message[0]);
		writeData(1, cstr2, strlen(cstr2));*/

		messageWithLength.append(std::to_string(message.length()));
		messageWithLength.append(message);

		
		//std::cout << "HERE: " << messageWithLength << "\n";

		char cstr[messageWithLength.size() + 1];
		strcpy(cstr, &messageWithLength[0]);
		


		writeData(sock, cstr, strlen(cstr));
		//writeData(STDOUT_FILENO, cstr, strlen(cstr));

		//updateGameMonitor();
	}
	else {
		//wysylam 1* po refresh encode()
		updateWaitingMonitor();
	}
	//writeData(STDOUT_FILENO, "a", 1);
	//printf("a");
}

decodedMessage decodeMessage(std::string strBuffer, int received) {

	//std::cout << "MESSAGE BEGGINING: " << strBuffer << "\n";

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

		MESSAGE_QUEUE += strBuffer.substr(finish + 1, strBuffer.length());

	}
	
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
	//std::cout <<"dm type: "<<dM.commandType<<"\ndm msg: "<< dM.commandMessage << std::endl;
	//std::cout << "MESSAGE END: " << MESSAGE_QUEUE << "\n";

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
		//std::cout << "a";

		//std::cout << MESSAGE_QUEUE;
		// Wywołanie poll - czekanie na wystąpnienie zdarzenia (w tym 
		// przypadku możliwości odczytu danych bez blokowania)
		int ready;
		//std::cout <<"MESSAGEQUELEN: "<< std::to_string(MESSAGE_QUEUE.length()) << std::endl;
		if (MESSAGE_QUEUE == "")
		{
			ready = poll(desc, 3, -1);
		}
		
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
				//std::cout << "MESSAGE_QUEUE FROM THE INSIDE: " << MESSAGE_QUEUE << "\n";
				// jeśli program raportuje możliwość odczytu, odczytaj dane
				received = readData(desc[i].fd, buffer, 255);

				// jeśli przyszły z standardowego wejścia, zapisz do funkcji kodujacej i wysylajacej,
				// wpw wyslij do funkcji odczytujacej
				//auto whereToWrite = desc[i].fd == STDIN_FILENO ? sock : STDOUT_FILENO;

				//PART FOR SENDING
				if (desc[i].fd == STDIN_FILENO) {
					//std::cout << "CHECK: ";// << buffer << "\n";
					encodeMessage(sock, buffer, received);
				}
				//PART FOR RECEIVING 
				else {
					//buffer od 0 do received
					//std::cout << "RECEIVED: " << std::to_string(received)<< "\n";

					char subbuffer[received];
					for (int i = 0; i < received; i++)
					{
						subbuffer[i] = buffer[i];
					}
					//std::cout << "subbuffer size : " << std::to_string(strlen(subbuffer)) << "\n";

					std::string strBuffer(subbuffer); //buffer[0;received]
					std::string finalBuffer = "";
													  
					for (int i = 0; i < received; i++)
					{
						finalBuffer += strBuffer[i];
					}
				    //std::cout << "strBuffer size : " << std::to_string(strBuffer.length()) << "\n";

					//std::cout << "MESSAGE BEGGINING A: " << MESSAGE_QUEUE << "\n";
					MESSAGE_QUEUE += finalBuffer;
					//std::cout << "MESSAGE BEGGINING B: " << MESSAGE_QUEUE << "\n";

					//std::cout << MESSAGE_QUEUE << "\n";
					//wzorzecwiadomosci 
					//regex wzorzec( "2*" );
				}
			}
			if (MESSAGE_QUEUE != "") {
				decodedMessage receivedData = decodeMessage(MESSAGE_QUEUE, received);
				
				
				std::string debugString = "" + receivedData.commandLength + ";" + receivedData.commandType + ";" + receivedData.commandMessage + "*";
				char cstr[debugString.size()+1];
				strcpy(cstr, &debugString[0]);
				//writeData(STDOUT_FILENO, cstr, sizeof(cstr)); 

				/*convert string to char array and print the result
				char cstr[receivedData.commandLength.size() + 1];
				strcpy(cstr, &receivedData.commandLength[0]);
				writeData(STDOUT_FILENO, cstr, sizeof(cstr));*/
				//response regards game status ex 5;1;210 - go to game status and set lives to 10
				if (receivedData.commandType == "1") {
					//inicjalizacja
					if (receivedData.commandMessage == "1") {
						GAMESTATUS = "1";

						///POWODUJE BLAD PO ODKOMENTOWANIU NIE WIADOMO CZEMU
						//updateInitMonitor();
					}
					//gra -> regex po 2 jest ilosc zyc
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
}
