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
#include <vector>
#include <bits/stdc++.h> 



////Z poprzedniej gry dorobic dwa pola do kazdego z widokow (LAST GAME SCORE I PLAYER)
////Obsluga przyjmowania podsumowania gry
////obsluga odswiezania ekranu w inicie
////i w poczekalni tez
////obsluga X dla nowej rundy czy cos (kartka)
////update poczatku rundy
//odswiezanie ekranu w grze

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
std::string GAMESTATUS ="1",LIVES="10",TOP_SCORE="UNKNOWN",TOP_PLAYER="UNKNOWN",nPLAYERS="UNKNOWN",nSPECTATORS="UNKNOWN",POINTS="0",WORD="", WORDLENGTH="0",MESSAGE_QUEUE="",YOUR_PLAYER_ID="ERROR",LAST_GAME_TOP_PLAYER="UNKNOWN",LAST_GAME_TOP_SCORE="UNKNOWN";
int intWORDLENGTH = 0, REWARD=10;
char LAST_LETTER_GUESS='-';


struct decodedMessage
{
	std::string commandLength;
	std::string commandType;
	std::string commandMessage;
};

/*std::string updateStrInt(std::string VAR, std::string VAL){
	
	int  tmpVAR= atol(VAR.c_str());
	int tmpVAL= atol(VAL.c_str());
	tmpVAR = tmpVAL;
	return std::to_string(tmpVAR);
}*/


void writeData(int fd, char* buffer, ssize_t count) {
	auto ret = write(fd, buffer, count);
	if (ret == -1) error(1, errno, "write failed on descriptor %d", fd);
	if (ret != count) error(0, errno, "wrote less than requested to descriptor %d (%ld/%ld)", fd, count, ret);
}

void updateInitMonitor() {
	std::string theString = "";
	theString += std::string("\033[2J") + "\033[0;0f"+"############################ THE HANGMAN #############################\n" + "YOU ARE PLAYER NUMBER: "+YOUR_PLAYER_ID+"\n######################################################################\n"+"Waiting for players to join\nCurrently there are: " + nSPECTATORS + " players in the waiting room\n\n" + "Press ENTER to refresh \nAnd remember that patience is key...\n""\n############################HALL OF FAME##############################\n"+"Last game won by: "+LAST_GAME_TOP_PLAYER+"\nWith the score of "+LAST_GAME_TOP_SCORE+" points"+"\n######################################################################\n";
	printf ("%s",theString.c_str());
}

void updateGameMonitor() {
	std::string theString="";
	theString+= std::string("\033[2J") + "\033[0;0f"+"############################ THE HANGMAN #############################\n"+ + "YOU ARE PLAYER NUMBER: "+YOUR_PLAYER_ID+"      Players to beat: "+nPLAYERS+"\n######################################################################\n"+"Current Word: " + WORD + "\nCurrently best score: " +TOP_SCORE +"\nYour points: " + POINTS + "\nLives: " + LIVES + "\n" + "Last game won by: "+LAST_GAME_TOP_PLAYER+"\nWith the score of "+LAST_GAME_TOP_SCORE+" points \nInsert 1 letter to guess it, or more to go for the word : \n";
	printf("%s",theString.c_str());
}

void updateWaitingMonitor() {
	std::string theString = "";
	theString += std::string("\033[2J") + "\033[0;0f" + "############################ THE HANGMAN #############################\n" + + "YOU ARE PLAYER NUMBER: "+YOUR_PLAYER_ID+"\n######################################################################\n"+"Current Top Score: " +TOP_SCORE + " & it belongs to player nuber: "+TOP_PLAYER+"\n" + "Number of players in the game: " + nPLAYERS + "\n" + "Number of spectators watching: " + nSPECTATORS + "\n" + "press ENTER to refresh the screen\n"+"\n############################HALL OF FAME##############################\n"+"Last game won by: "+LAST_GAME_TOP_PLAYER+"\nWith the score of "+LAST_GAME_TOP_SCORE+" points"+"\n######################################################################\n";
	printf("%s",theString.c_str());
}

void encodeMessage(int sock, char * buffer, int received) {

	//-1 zeby uciac \n na koncu
	std::string finalBuffer2(buffer, received-1);

	std::string messageWithLength;
		std::string message;
		//std::string strBuffer(buffer);
		
		//FINAL BUFFER MA NA KONCU \n
		message.append(";1;");
		message.append(finalBuffer2);
		message.append("*");
		if(finalBuffer2.length()==1){
			LAST_LETTER_GUESS=finalBuffer2[0];
		}

		messageWithLength.append(std::to_string(message.length()));
		messageWithLength.append(message);

		//check if user is not inserting ILLEGAL characters with ASCII
		if (GAMESTATUS=="2"){
			for(int i=0;i<finalBuffer2.length();i++){
				if (finalBuffer2[i]<65||(finalBuffer2[i]<97&&finalBuffer2[i]>90)||finalBuffer2[i]>122){
					int  tmp= atol(LIVES.c_str());
					tmp -= 2;
					LIVES = std::to_string(tmp);
					updateGameMonitor();
					std::cout<<"Illegal letter detected! try only (A-Z or a-z) \nInsert valid letter: ";
					return;
				}
			}
		}
		///UWAGA
		writeData(sock, messageWithLength.data(), strlen(messageWithLength.c_str()));
}

std::vector<std::string>  decodeMessageLoad(std::string message){
	std::vector<std::string> message_items;
	for(int i=0;i<message.length();i++){
			int temp=i;
			std::string item="";
			for(temp;temp<message.length();temp++){
				if (message[temp]=='-') break;
				item+=message[temp];
				i++;
			}
			message_items.push_back(item);
	}
	return message_items;
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
	///CHANGED FROM ELSE IF TO CHECK
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
				
				//std::cout<<finalBuffer3;

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
					std::vector<std::string> messageLoad = decodeMessageLoad(receivedData.commandMessage);

					
					//inicjalizacja
					if (messageLoad[0] == "1") {
						GAMESTATUS = "1";
						if (messageLoad[1]!="X"&&messageLoad[1]!="x"){
							YOUR_PLAYER_ID=messageLoad[1];
						}
						updateInitMonitor();
					}
					//poczekalnia 
					else if (messageLoad[0] == "3") {
						GAMESTATUS = "3";
						updateWaitingMonitor();
					}
					else {
						//uzupelnia string WORD podkresleniami tyle ile jest po liczbie 2
						GAMESTATUS = "2";
						LIVES = messageLoad[1];
						updateGameMonitor();
					}
				}
				//response regards the number of letters in the new word ex 6;2;30* - 30 letters in the word
				else if (receivedData.commandType == "2") {
					std::vector<std::string> messageLoad = decodeMessageLoad(receivedData.commandMessage);
					WORDLENGTH = messageLoad[0];
					char cstr[WORDLENGTH.size() + 1];
					strcpy(cstr, &WORDLENGTH[0]);
					intWORDLENGTH = atol(cstr);
					WORD="";
					for (int i = 0; i < intWORDLENGTH; i++)
					{
						WORD += "_";
					}
					TOP_SCORE=messageLoad[1];
					TOP_PLAYER=messageLoad[2];
					updateGameMonitor();
				}
				//response regards the correctness of chosen letter 
				else if (receivedData.commandType == "3") {
					if (receivedData.commandMessage == "") {
						int  tmp= atol(LIVES.c_str());
						tmp -= 1;
						LIVES = std::to_string(tmp);
						//LIVES -= 1;
					}
					//get letter positions and update the word array
					else {
						std::vector<std::string> positions = decodeMessageLoad(receivedData.commandMessage);
						for (int i=0;i<positions.size();i++)
						{
							int intPosition= atol(positions[i].c_str());
							WORD[intPosition]=LAST_LETTER_GUESS;
							printf("%s\n", positions[i].c_str());
						}
					}
					updateGameMonitor();
				}
				//response regards the correctness of sent word 
				else if (receivedData.commandType == "4") {		
					// add points
					if (receivedData.commandMessage == "1"){
						int tmp = atol(POINTS.c_str());
						tmp+=REWARD;
						POINTS = std::to_string(tmp);
					}
					// subtract one life
					else{
						int  tmp= atol(LIVES.c_str());
						tmp -= 1;
						LIVES = std::to_string(tmp);
					}
					updateGameMonitor();
				}
				//response has the summary of the game
				else if (receivedData.commandType == "5") {	
					std::vector<std::string> messageLoad = decodeMessageLoad(receivedData.commandMessage);
					LAST_GAME_TOP_SCORE=messageLoad[0];
					LAST_GAME_TOP_PLAYER=messageLoad[1];
				}
				//response contains data to refresh init screen
				else if (receivedData.commandType == "6") {	
					//std::vector<std::string> messageLoad = decodeMessageLoad(receivedData.commandMessage);
					nSPECTATORS=receivedData.commandMessage;
					updateInitMonitor();
				}
				//response contains data to refresh wating room	
				else if (receivedData.commandType == "7") {	
					std::vector<std::string> messageLoad = decodeMessageLoad(receivedData.commandMessage);
					nSPECTATORS=messageLoad[0];
					nPLAYERS=messageLoad[1];
					TOP_SCORE=messageLoad[2];
					TOP_PLAYER=messageLoad[3];
					updateWaitingMonitor();
				}
				//catching the type for number of players after each request sent from player
				else if (receivedData.commandType == "8"){
					nPLAYERS=receivedData.commandMessage;
					updateGameMonitor();
				}	

			}
		}
}
