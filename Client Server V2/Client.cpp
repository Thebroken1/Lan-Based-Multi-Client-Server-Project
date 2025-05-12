	#include <iostream>
	#include <WinSock2.h>
	#include <Ws2tcpip.h> 
	#include <conio.h>
	#include <windows.h>
	#include <thread>
	#include <vector>
	#include <string>
	#include <iphlpapi.h>	
	#include <mutex>
	#include <atomic>
	#pragma comment(lib, "Ws2_32.lib")
	#pragma comment(lib, "iphlpapi.lib")

std::atomic<bool> running(true);
	std::vector<std::string>messagesConstant;
	std::mutex messagesConstantMutex;

	bool clientStart(WSADATA& wsa) {

		if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {

			std::cout << "Startup failed";
			return false;

		}

		else {

			return  true;

		}

	}
	
	std::string serverIPFinder() {
		WSADATA wsa;
		WSAStartup(MAKEWORD(2, 2), &wsa);
		SOCKET udpSocket = socket(AF_INET, SOCK_DGRAM, 0);

		if (udpSocket == INVALID_SOCKET) {

			std::cout << "ERROR WITH SOCKET" << WSAGetLastError() << "\n";
			return "";
		}

		sockaddr_in recieverAddress{};
		recieverAddress.sin_family = AF_INET;
		recieverAddress.sin_port = htons(1111);
		recieverAddress.sin_addr.s_addr = INADDR_ANY;


		if (bind(udpSocket, (sockaddr*)&recieverAddress, sizeof(recieverAddress)) < 0) {
			std::cout << "ERROR" << WSAGetLastError();
			closesocket(udpSocket);
			return "";
		}

		char buffer[1024];
		sockaddr_in senderAddr;
		int senderLen = sizeof(senderAddr);

		int bytesRecieved = recvfrom(udpSocket, buffer, sizeof(buffer) - 1, 0, (sockaddr*)&senderAddr, &senderLen);

		if (bytesRecieved > 0) {
			buffer[bytesRecieved] = '\0';
			std::string msg(buffer);
			if (msg.find("SERVER_IP_ADDRESS:") == 0) {
				std::string serverIP = msg.substr(18);
				closesocket(udpSocket);
				return serverIP;
			}
		}

		closesocket(udpSocket);
		return "";
	}

	SOCKET clientConnection(sockaddr_in &server) {

		SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
		if (sock == INVALID_SOCKET) {

			std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
			WSACleanup();
			return 1;

		}

		std::string serverIP = serverIPFinder();
		server.sin_family = AF_INET;
		server.sin_port = htons(1111);
		const char* ip = "10.112.40.60";//rem to change this to designated ip

		if (inet_pton(AF_INET, serverIP.c_str(), &server.sin_addr) <= 0) {

			std::cerr << "Invalid address or address not supported" << std::endl;
			return 1;

		}
		return sock;
	}

	void moveCursorToTop() {
		HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
		COORD coordScreen = { 0, 0 };
		SetConsoleCursorPosition(hConsole, coordScreen);
	}

	void redrawScreen() {
		moveCursorToTop();

		std::lock_guard<std::mutex> lock(messagesConstantMutex);

		DWORD written;
		HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
		CONSOLE_SCREEN_BUFFER_INFO csbi;
		GetConsoleScreenBufferInfo(hConsole, &csbi);
		FillConsoleOutputCharacter(hConsole, ' ', csbi.dwSize.X * csbi.dwSize.Y, { 0,0 }, &written);
		SetConsoleCursorPosition(hConsole, { 0,0 });

		for (const auto& msg : messagesConstant) {
			std::cout << msg << "\n";
		}

		std::cout << "Enter Message(type '/exit' to exit,'/help' for help): ";

	}

	void chat(SOCKET sock,std::atomic<bool>& running) {

		std::string mes, user;
		const std::string constmsg = "Enter Message(type '/exit' to exit,'/help' for help): ";
		const char* message;
		const char* clientName = {};

		std::cout << "Enter Username: ";
		std::getline(std::cin, user);
		clientName = user.c_str();
		if (clientName == NULL) {
			std::cout << "\nERROR GETTING USERNAME, restart program:\n";
			return;
		}
		send(sock, clientName, static_cast<int>(strlen(clientName)), 0);
		std::cout << constmsg;
		while (running) {
			                           

			std::getline(std::cin, mes);
			if (mes == "/exit") {
				running = false;
				std::cout << "Closing";
				shutdown(sock, SD_BOTH);
				closesocket(sock);
				WSACleanup();
				return;
			}
			message = mes.c_str();

			send(sock, message, static_cast<int>(strlen(message)), 0);

			{
				std::lock_guard<std::mutex>lock(messagesConstantMutex);
				messagesConstant.push_back(user+": "+mes);
			}
			redrawScreen();

		}
	}

	void recieveMessages(SOCKET sock, std::atomic<bool>& running) {
		char buffer[2048] = {};
		int serverReply;
		while(running){
			serverReply = recv(sock, buffer, sizeof(buffer) - 1, 0);
			if (serverReply == SOCKET_ERROR) {
				std::cout << "Server Down";
				running = false;
				std::cout << "Closing";
				shutdown(sock, SD_BOTH);
				closesocket(sock);
				WSACleanup();
				return;
			}

			if (serverReply == 0) {
				std::cout << "Server Disconnected";
				running = false;
				std::cout << "Closing";
				shutdown(sock, SD_BOTH);
				closesocket(sock);
				WSACleanup();
				return;
			}
			if (serverReply > 0) {

				buffer[serverReply] = '\0';
				{
					std::lock_guard<std::mutex>lock(messagesConstantMutex);
					messagesConstant.push_back(std::string(buffer))	;
				}

			}
			std::cout << "\n" << buffer << std::endl;
			memset(buffer, 0, sizeof(buffer));
			redrawScreen();
		}
	}

	void cleanUp(SOCKET sock) {
		closesocket(sock);
		WSACleanup();
	}
	
	int main() {

		WSADATA wsa;
		SOCKET sock;
		sockaddr_in server = {};
		clientStart(wsa);

		sock = clientConnection(server);
		std::atomic<bool> running(true);
		if (connect(sock, (sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
			std::cerr << "Connection failed with error: " << WSAGetLastError() << std::endl;
		}
		std::thread messageReciever(recieveMessages, sock, std::ref(running));
		std::thread chatThread(chat, sock, std::ref(running));
		chatThread.join();
		running = false;
		messageReciever.join();
		//chat(sock);
	
		cleanUp(sock);


	}