#include <iostream>
#define SECURITY_WIN32

#include <winsock2.h>  
#include <schannel.h>
#include <security.h>
#include <sspi.h>

#define SECURITY_WIN32
#define IO_BUFFER_SIZE  0x10000
#define DLL_NAME TEXT("Secur32.dll")
#define NT4_DLL_NAME TEXT("Security.dll")

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <winsock2.h>
#include <wincrypt.h>
#include <wintrust.h>
#include <schannel.h>

#pragma comment(lib, "WSock32.Lib")
#pragma comment(lib, "Crypt32.Lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "MSVCRTD.lib")
#pragma comment(lib, "Secur32.lib")

#include <string>
#include <thread>
#include <fstream>
#include <json/json.h> 
#include <ws2tcpip.h> // For getaddrinfo
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include <cstdlib> // for the system function

#include <maxminddb.h>

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#pragma comment(lib, "ws2_32.lib")  // Link against the Winsock library

// Define the server's IPv4 address and port to listen on
#define SERVER_IP "192.168.1.129" // Use your desired IPv4 address
#define SERVER_PORT 27021
//23.241.31.105, 27022
//http://192.168.1.129:27022
//https://23.241.31.105:27022

int numberOfClientsJoined = 0;

Json::CharReaderBuilder jsonReader;
Json::Value jsonData;
std::string jsonErrors;

bool white_turn = true;
bool gameStarted = false;

class vec2 {
public:
	float x, y = 0;
};
void print(std::string x) {
	std::cout << x << std::endl;
}
class Actor {
public:
	vec2 pos;
	int id;
	bool white;

	// Overload the << operator to print an Actor
	friend std::ostream& operator<<(std::ostream& os, const Actor& actor) {
		os << "Actor ID: " << actor.id << " Position: (" << actor.pos.x << ", " << actor.pos.y << ")";
		return os;
	}
	// Convert Actor data to a JSON-formatted string
	std::string actorToJson(bool white_turn) {
		// Create a RapidJSON document
		rapidjson::Document document;
		document.SetObject();

		// Add data to the document
		rapidjson::Document::AllocatorType& allocator = document.GetAllocator();
		document.AddMember("white_turn", white_turn, allocator);
		document.AddMember("id", id, allocator);
		document.AddMember("x", pos.x, allocator);
		document.AddMember("y", pos.y, allocator);
		//document.AddMember("white", white, allocator);

		// Serialize the document to a string
		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		document.Accept(writer);

		// Convert the string buffer to a string
		std::string json_string = buffer.GetString();

		return json_string;
	}

};

class Client {
public:
	Actor actor;
	SOCKET clientSocket;
	int id = 0;
	bool whitePlayer;

	// Overload to send a string to the client
	void sendToClient(const std::string& string_send) {
		// Send data
		int bytesSent = send(clientSocket, string_send.c_str(), string_send.length(), 0);
		if (bytesSent == SOCKET_ERROR) {
			std::cerr << "Sending data failed." << std::endl;
			closesocket(clientSocket);
			WSACleanup();
		}
		else {
			std::cout << "Sent: " << string_send << std::endl;
		}
	}

	// Overload to send a bool to the client
	void sendToClient(bool boolSend) {
		std::string string_send = boolSend ? "1" : "0";
		sendToClient(string_send);
	}

};

std::vector<Client> clients; // Vector to store client instances

void handleClient(SOCKET clientSocket) {
	std::cout << "Handling client" << std::endl;

	Client* client = new Client();
	client->clientSocket = clientSocket;

	numberOfClientsJoined++;
	std::cout << "clients joined: " << numberOfClientsJoined << std::endl;
	client->id = numberOfClientsJoined - 1;

	clients.push_back(*client);
	// Calculate c_id based on the index in the vector

	clients[client->id].whitePlayer = (clients[client->id].id == 1) ? true : false;

	if (numberOfClientsJoined > 2) {
		numberOfClientsJoined = 0;
	}
	
	else if (numberOfClientsJoined == 2) {

		gameStarted = true;
		std::cout << "\n\nTwo Players Present! Begin Game... " << std::endl;
		for (Client& loopClient : clients) {
			//loopClient.sendToClient(loopClient.whitePlayer);
			std::cout << "Player id = " << loopClient.id << " ; White Player = " << loopClient.whitePlayer << std::endl;
		}
		clients[0].sendToClient(std::to_string(0));
		clients[1].sendToClient(std::to_string(1));
	}
	

	std::string receivedData;
	char buffer[300];

	bool canRead = true;

	while (canRead) {
		int bytesReceived = recv(clients[client->id].clientSocket, buffer, sizeof(buffer), 0);
		if (bytesReceived <= 0) {
			std::cerr << "Client disconnected." << std::endl;
			canRead = false;
			break;
		}

		receivedData += std::string(buffer, bytesReceived);

		// Check if the received data is a complete JSON object
		size_t lastBrace = receivedData.find_last_of('}');
		if (lastBrace != std::string::npos) {
			std::string jsonDataString = receivedData.substr(0, lastBrace + 1);
			receivedData = receivedData.substr(lastBrace + 1);

			std::istringstream bufferStream(jsonDataString);

			if (!Json::parseFromStream(jsonReader, bufferStream, &jsonData, &jsonErrors)) {
				std::cerr << "JSON parsing error: " << jsonErrors << std::endl;
				continue;  // Skip processing this batch of data
			}

			if (jsonData.isMember("id") && jsonData.isMember("pos")) {
				clients[client->id].actor.id = jsonData["id"].asInt();
				const Json::Value& posData = jsonData["pos"];
				if (posData.isMember("x") && posData.isMember("y")) {
					clients[client->id].actor.pos.x = posData["x"].asFloat();
					clients[client->id].actor.pos.y = posData["y"].asFloat();
					print(std::to_string(clients[client->id].actor.pos.x));
				}
				else {
					std::cerr << "Invalid position data for actor." << std::endl;
					continue;  // Skip processing this batch of data
				}
			}
			else {
				std::cerr << "Invalid data format for actor." << std::endl;
				continue;  // Skip processing this batch of data
			}
			if (clients[client->id].actor.id <= 16) {
				clients[client->id].actor.white = true;
			}
			else {
				clients[client->id].actor.white = false;
			}
			white_turn = !white_turn;

			//send to both players updated white_turn
			clients[0].sendToClient(clients[client->id].actor.actorToJson(white_turn));
			clients[1].sendToClient(clients[client->id].actor.actorToJson(white_turn));
		}
	}
	if (clients.size() == 2) {
		clients[0] = clients[1];
		clients.pop_back(); // Remove the last element which is now a duplicate of the first
	}
	//system("cls");
	numberOfClientsJoined -= 1;
	delete client;
	std::cout << "Number of Clients = " << numberOfClientsJoined << std::endl;
	return; //end thread
}

SOCKET setupServerSocket() {
	SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0); // Use AF_INET for IPv4
	if (serverSocket == INVALID_SOCKET) {
		std::cerr << "Socket creation failed." << std::endl;
	}
	else {
		std::cout << "Socket Created." << std::endl;
	}
	sockaddr_in serverAddr; // Use sockaddr_in for IPv4
	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(SERVER_PORT); // Choose a port number
	if (inet_pton(AF_INET, SERVER_IP, &(serverAddr.sin_addr)) <= 0) {
		std::cerr << "Invalid address or address family" << std::endl;
		closesocket(serverSocket);
		WSACleanup();
	}
	if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
		std::cerr << "Binding failed." << std::endl;
		closesocket(serverSocket);
	}
	else {
		std::cout << "Binding Completed" << std::endl;
	}
	return serverSocket;
}
void getClientIpPort(SOCKET clientSocket) {
	sockaddr_in clientAddr; // Use sockaddr_in for IPv4
	int addrLen = sizeof(clientAddr);
	getpeername(clientSocket, (struct sockaddr*)&clientAddr, &addrLen);

	char clientIP[INET_ADDRSTRLEN]; // Use INET_ADDRSTRLEN for IPv4 addresses
	inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, sizeof(clientIP));
	int clientPort = ntohs(clientAddr.sin_port); // For IPv4

	std::cout << "\n\nAccepted connection from client IP: " << clientIP << " at port: " << clientPort << std::endl;
}
SOCKET acceptClient(SOCKET serverSocket) {
	// Accept incoming client connection
	SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);
	if (clientSocket == INVALID_SOCKET) {
		std::cerr << "Accepting connection failed." << std::endl;
		closesocket(clientSocket);
		WSACleanup();
		return NULL;
	}
	else {
		getClientIpPort(clientSocket);
		return clientSocket;
	}
}

void run_socket() {
	std::cout << "Running socket";
	bool serverRunning = true;

	SOCKET serverSocket = setupServerSocket();

	while (serverRunning) {
		if (listen(serverSocket, 1) == SOCKET_ERROR) {
			std::cerr << "Listening failed." << std::endl;
			closesocket(serverSocket);
		}
		else {
			std::cout << "Listening..." << std::endl;
			SOCKET clientSocket = acceptClient(serverSocket);
			if (!clientSocket == NULL) {
				std::thread client_thread(handleClient, clientSocket);
				client_thread.detach(); // Wait for the thread to finish
			}
		}
	}
	// Close the server socket and perform cleanup
	closesocket(serverSocket);
	WSACleanup();
}

void getLocalIpAdress() {
	char hostName[NI_MAXHOST];
	if (gethostname(hostName, NI_MAXHOST) == 0) {
		struct addrinfo* result = nullptr;
		struct addrinfo hints;
		ZeroMemory(&hints, sizeof(hints));
		hints.ai_family = AF_INET; // Request IPv4 address

		if (getaddrinfo(hostName, nullptr, &hints, &result) == 0) {
			struct sockaddr_in* addr = reinterpret_cast<struct sockaddr_in*>(result->ai_addr);
			char ipBuffer[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &(addr->sin_addr), ipBuffer, INET_ADDRSTRLEN);
			std::cout << "Local IPv4 Address: " << ipBuffer << " on port: " << SERVER_PORT << std::endl;
			freeaddrinfo(result);
		}
		else {
			std::cerr << "getaddrinfo failed." << std::endl;
		}
	}
	else {
		std::cerr << "gethostname failed." << std::endl;
	}
}

int main() {
	std::cout << "Running Server Websocket for Chess Game";
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		std::cerr << "WSAStartup Failed." << std::endl;
	}
	else {
		std::cout << "WSAStartup Success." << std::endl;
	}

	getLocalIpAdress();
	run_socket();

	return 0;
}
