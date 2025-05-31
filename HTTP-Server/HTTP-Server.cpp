#include <iostream>
#include <winsock2.h>
#include <thread>
#include <map>
#include <vector>

#pragma comment(lib, "Ws2_32.lib")

typedef struct HTTPRequest {
    std::string method;
    std::string path;
    std::map < std::string, std::string > args;
    std::map < std::string, std::string > headers;
    std::string body;
} HTTPRequest;

int createSocket(SOCKET& serverSocket);

int bindServer(SOCKET& serverSocket, int port = 8081);

int handelConnection(SOCKET& serverSocket,SOCKET clientSocket,sockaddr_in addr, int addrlen);

void parseRequest(std::string rawr, HTTPRequest& request);

std::string trimString(std::string s);

int main() {
    SOCKET serverSocket;

    if (createSocket(serverSocket) != 0) return 1;

    if (bindServer(serverSocket) != 0) return 1;

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server listening on port 8081..." << std::endl;

    while (true) {
        sockaddr_in clientAddr;
        int clientSize = sizeof(clientAddr);
        SOCKET clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientSize);
        std::thread(handelConnection, std::ref(serverSocket), clientSocket, clientAddr, clientSize).detach();
    }

    closesocket(serverSocket);
    WSACleanup();

    return 0;
}



int createSocket(SOCKET& serverSocket) {
    WSADATA wsaData;
    int wsaResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (wsaResult != 0) {
        std::cerr << "WSAStartup failed: " << wsaResult << std::endl;
        return 1;
    }


    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    return 0;
}

int bindServer(SOCKET& serverSocket, int port) {
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);          

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    return 0;
}

int handelConnection(SOCKET& serverSocket,SOCKET clientSocket, sockaddr_in addr, int addrlen)
{
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Accept failed: " << WSAGetLastError() << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Client connected!" << std::endl;

    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(clientSocket, &readSet);

    struct timeval timeout { 10, 0 };
    while (true)
    {
        int result = select(0, &readSet, nullptr, nullptr, &timeout);
        if (result > 0 && FD_ISSET(clientSocket, &readSet)) {
            char buffer[1024];
            int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
            if (bytesReceived > 0) {

                HTTPRequest request;

                parseRequest(std::string(buffer, bytesReceived), request);

                std::cout << "Method: " << request.method << std::endl;
                std::cout << "Path: " << request.path << std::endl;
                std::cout << "Body: " << request.body << std::endl;

                std::cout << "HEADERS:\n";
                for (const auto& pair : request.headers) {
                    std::cout << "K: " << pair.first << " , V: " << pair.second << std::endl;
                }
                std::cout << "ARGS:\n";
                for (const auto& pair : request.args) {
                    std::cout << "K: " << pair.first << " , V: " << pair.second << std::endl;
                }

                std::string httpResponse =
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/plain\r\n"
                    "Content-Length: 13\r\n"
                    "\r\n"
                    "Hello, world!";

                send(clientSocket, httpResponse.c_str(), httpResponse.size(), 0);
            }
        }
        else break;
    }
    std::cout << "client disconnected...\n";
    closesocket(clientSocket);
    return 0;
}

void parseRequest(std::string rawr, HTTPRequest& request) {
    std::cout << rawr << std::endl;
    int pos = 0;
    int len = rawr.find("/");
    request.method = trimString(rawr.substr(0,len));
    rawr.erase(rawr.begin(), rawr.begin() + len);
    len = rawr.find("HTTP");

    std::string path = rawr.substr(0, len);

    if (path.find("?") != std::string::npos) {
        len = path.find("?");
        request.path = trimString(path.substr(0, len));
        path.erase(path.begin(), path.begin() + len + 1);
        while (true)
        {
            if (path.find("&") != std::string::npos) {
                len = path.find("&");
                std::string arg = trimString(path.substr(0, len));
                std::string k = arg.substr(0, arg.find("="));
                std::string v = arg.substr(arg.find("=") + 1, arg.length() - k.length());
                request.args[trimString(k)] = trimString(v);
                path.erase(path.begin(), path.begin() + len + 1);
            }
            else {
                std::string k = path.substr(0, path.find("="));
                std::string v = path.substr(path.find("=") + 1, path.length() - k.length());
                request.args[trimString(k)] = trimString(v);
                break;
            }
        }
    } else request.path = trimString(rawr.substr(0, len));

    len = rawr.find("\r\n") + 2;
    rawr.erase(rawr.begin(), rawr.begin() + len);
    while (true)
    {
        len = rawr.find("\r\n") + 2;
        std::string header = rawr.substr(0, len);
        if (!trimString(header).empty()) {
            std::string k = header.substr(0, header.find(":"));
            std::string v = header.substr(header.find(":") + 1, header.length() - k.length());
            request.headers[trimString(k)] = trimString(v);
            rawr.erase(rawr.begin(), rawr.begin() + len);
        }
        else break;
    }

    request.body = trimString(rawr);
}

std::string trimString(std::string s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
        }));

    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
        }).base(), s.end());

    return s;
}


