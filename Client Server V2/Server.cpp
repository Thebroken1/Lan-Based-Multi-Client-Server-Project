#include <iostream>
#include <winsock2.h>
#include <Ws2tcpip.h> 
#include <conio.h>
#include <thread>
#include <vector>
#include <atomic>
#include<mutex>
#include <iphlpapi.h>
#include <cctype>


#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

std::atomic<int> count = 0;
std::vector<std::string>connectedUsers;
std::vector<SOCKET>clientSockets;
std::mutex userMutex,socketMutex;

bool serverStart(WSADATA &wsa){

	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {

		std::cout << "Startup failed";
		return false;

	}

	else {

		return  true;

	}

}

std::string IPv4Getter() {
	char ipString[INET_ADDRSTRLEN] = {};
	ULONG bufferSize = 15000;
	IP_ADAPTER_ADDRESSES* addresses = (IP_ADAPTER_ADDRESSES*)malloc(bufferSize);

	if (GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, NULL, addresses, &bufferSize) == ERROR_SUCCESS) {

		for (IP_ADAPTER_ADDRESSES* adapter = addresses; adapter != NULL; adapter = adapter->Next) {
			if (adapter -> OperStatus == IfOperStatusUp && adapter->IfType == IF_TYPE_IEEE80211) {
				IP_ADAPTER_UNICAST_ADDRESS* unicast = adapter->FirstUnicastAddress;
				while (unicast) {
					if (unicast -> Address.lpSockaddr -> sa_family == AF_INET) {
						sockaddr_in* sa_in = reinterpret_cast<sockaddr_in*>(unicast->Address.lpSockaddr);
						inet_ntop(AF_INET, &(sa_in->sin_addr), ipString, INET_ADDRSTRLEN);
						free(addresses);
						return std::string(ipString);
					}
					unicast = unicast->Next;
				}
			}
		}

	}

	free(addresses);
	return "Unable to retrieve IP";
}

SOCKET connection(sockaddr_in& server) {

	SOCKET serverSock = socket(AF_INET, SOCK_STREAM, 0);
	if (serverSock == INVALID_SOCKET) {

		std::cout << WSAGetLastError;
		return INVALID_SOCKET;

	}

	std::string ip = IPv4Getter();


	server.sin_family = AF_INET;
	server.sin_port = htons(1111);
	//server.sin_addr.s_addr = ip; //changed to public ip

	if (inet_pton(AF_INET, ip.c_str(), &(server.sin_addr)) <= 0) {

		std::cerr << "Invalid";
		return INVALID_SOCKET;

	}

	if (bind(serverSock, (sockaddr*) &server, sizeof(server)) == SOCKET_ERROR) {

		std::cout << "Bind failed" << WSAGetLastError() << "\n";
		return INVALID_SOCKET;

	}

	if (listen(serverSock, 3) == SOCKET_ERROR) {

		std::cout << "Listener Failed" << WSAGetLastError() << "\n";
		return INVALID_SOCKET;

	}

	std::cout << "Server Started\n";
	return serverSock;
}

void broadCastingServerIp() {
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);
	SOCKET udpSocket = socket(AF_INET, SOCK_DGRAM, 0);

	if (udpSocket == INVALID_SOCKET) {
		std::cout << "ERROR BROADCASTING" << WSAGetLastError() << "\n";
		return;
	}

	BOOL broadcasting = true;

	setsockopt(udpSocket ,SOL_SOCKET, SO_BROADCAST, (char*)&broadcasting, sizeof(broadcasting));

	sockaddr_in broadcastAddress{};
	broadcastAddress.sin_family = AF_INET;
	broadcastAddress.sin_port = htons(1111);
	broadcastAddress.sin_addr.s_addr = INADDR_BROADCAST;

	std::string ip = IPv4Getter();
	std::cout << "[Broadcast] Sending IP: " << ip << std::endl;

	std::string mess = "SERVER_IP_ADDRESS:" + ip;

	while (true) {

		sendto(udpSocket, mess.c_str(), mess.length(), 0, (sockaddr*)&broadcastAddress, sizeof(broadcastAddress));

		std::this_thread::sleep_for(std::chrono::seconds(3));
	}

	closesocket(udpSocket);

}

void userList(SOCKET clientSock) {

	std::string list = "\nConnected Users \n";

	{
		std::lock_guard<std::mutex> lock(userMutex);
		for (const auto& user : connectedUsers) {
			list += "-" + user + "\n";
		}
	}
	send(clientSock, list.c_str(), list.size(), 0);
}

void chat(SOCKET clientSock) {

	if (clientSock == INVALID_SOCKET) {

		std::cout << "ERROR CONNECTING" << WSAGetLastError() << "\n";

	}

	else {

		std::cout << "\nConnected to Client" << std::endl;

	}

	char buffer[4056] = {};
	char usernameBuffer[256] = {};
	std::string username, messageBroadcasted;
	int counter = 30000, byteReceived, inc = 0, usernameReceived, lowerCheck;

	setsockopt(clientSock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&counter, sizeof(counter));
	
	usernameReceived = recv(clientSock, usernameBuffer, sizeof(usernameBuffer) - 1, 0);
	if (usernameReceived > 0 && usernameReceived < sizeof(usernameBuffer)) {

		usernameBuffer[usernameReceived] = '\0';
		if (username.empty()) {
			username = usernameBuffer;
			{
				std::lock_guard<std::mutex> lock(userMutex);
				connectedUsers.push_back(username);

			}

		}


	}
	{
		std::lock_guard<std::mutex>lock(socketMutex);
		clientSockets.push_back(clientSock);
	}

	while (true) {
		memset(buffer, 0, sizeof(buffer));
		byteReceived = recv(clientSock, buffer, sizeof(buffer) - 1, 0);
		

		if (byteReceived == SOCKET_ERROR || byteReceived <= 0) {

			std::cout << "\nClient disconnected\n";
			count--;
			std::cout << "\nTotal Number of users: " << count<<"\n";

			std::lock_guard<std::mutex>lock(userMutex);
			connectedUsers.erase(
				std::remove(connectedUsers.begin(), connectedUsers.end(), username),
				connectedUsers.end()
			);
			break;

		}

		else {

			inc += 1;
			lowerCheck = tolower(byteReceived);
			if (byteReceived > 0 ) {

				buffer[byteReceived] = '\0';

			}

			if (strcmp(buffer, "/users") == 0) {
				userList(clientSock);
				continue;
				memset(buffer, 0, sizeof(buffer));
			}
			if (strcmp(buffer, "/help") == 0) {
				messageBroadcasted = "/users - displays current users\n/help - displays help page";
				send(clientSock, messageBroadcasted.c_str(), messageBroadcasted.size(), 0);
				continue;
				memset(buffer, 0, sizeof(buffer));
			}
			
			else{
			std::cout << "\nClient: " << username;
			std::cout << "\nResponse: " << buffer << "\n";
			messageBroadcasted = username + ": " + buffer;
			{
				std::lock_guard <std::mutex> lock (socketMutex);
				for (SOCKET sock : clientSockets) {
					if (sock != clientSock) {
						send(sock, messageBroadcasted.c_str(), messageBroadcasted.size(), 0);
						std::cout << "\n";
					}
				}
			}
			}
		}
	}


}

void Cleanup(SOCKET serverSock, SOCKET clientSock) {
	closesocket(serverSock);
	closesocket(clientSock);
	WSACleanup();
}

void handleClients(SOCKET clientSock) {

	chat(clientSock);
	closesocket(clientSock);
	std::lock_guard<std::mutex> lock(socketMutex);
	clientSockets.push_back(clientSock);
}

int main() {

	WSADATA wsa;
	SOCKET serverSock,clientSock;
	sockaddr_in server = {};
	sockaddr_in client = {};
	int clientSize = sizeof(client);
	BOOL opt = TRUE;
	int c;
	std::vector<std::thread>clientThread;

	serverStart(wsa);

	c = sizeof(sockaddr_in);
	serverSock = connection(server);

	std::thread broadcastingThread(broadCastingServerIp);
	broadcastingThread.detach();

	while(true){
	clientSock = accept(serverSock, (sockaddr*)&client, &c);
	if (clientSock == INVALID_SOCKET) {
		std::cerr << "Accept failed: " << WSAGetLastError() << std::endl;
	}
	else {
		std::cout << "\nClient connected!\n" << std::endl;
	}
	clientThread.emplace_back(std::thread(handleClients, clientSock));
	count++;
	std::cout << "\nTotal Number of users: "<<count;
	clientThread.back().detach();
	}
	Cleanup(serverSock, clientSock);

}
