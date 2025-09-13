// server.cpp
// Single-file C++ TCP server using Winsock2 and nlohmann::json (json.hpp).
// Put json.hpp in the same folder and add it to the project.

#define _CRT_SECURE_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <mutex>
#include <algorithm>

#pragma comment(lib, "Ws2_32.lib")

#include <nlohmann/json.hpp>
using json = nlohmann::json;

struct TodoItem {
    int id;
    std::string description;
    bool completed;
};

class Server {
public:
    Server(int port) : listenPort(port), nextId(1) {}
    bool start();
    void runAcceptLoop();

private:
    int listenPort;
    SOCKET listenSock = INVALID_SOCKET;
    std::vector<SOCKET> clients;
    std::mutex clientsMu;

    std::vector<TodoItem> todoList;
    std::mutex todoMu;
    int nextId;

    void handleClient(SOCKET clientSock);
    // process returns pair<responseJsonString, shouldBroadcast>
    std::pair<std::string, bool> processMessage(const std::string& msg);
    void broadcast(const std::string& message, SOCKET exclude = INVALID_SOCKET);
    void removeClient(SOCKET s);
};

bool Server::start() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "WSAStartup failed\n"; return false;
    }

    listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET) {
        std::cerr << "socket() failed\n"; return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY; // bind to all interfaces
    addr.sin_port = htons((unsigned short)listenPort);

    if (bind(listenSock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "bind() failed\n"; closesocket(listenSock); return false;
    }

    if (listen(listenSock, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "listen() failed\n"; closesocket(listenSock); return false;
    }

    std::cout << "Server listening on port " << listenPort << " ...\n";
    return true;
}

void Server::runAcceptLoop() {
    while (true) {
        sockaddr_in clientAddr{};
        int clientLen = sizeof(clientAddr);
        SOCKET client = accept(listenSock, (sockaddr*)&clientAddr, &clientLen);
        if (client == INVALID_SOCKET) {
            std::cerr << "accept() returned INVALID_SOCKET\n";
            continue;
        }

        {
            std::lock_guard<std::mutex> lk(clientsMu);
            clients.push_back(client);
        }

        std::thread(&Server::handleClient, this, client).detach();
    }
}

void Server::handleClient(SOCKET clientSock) {
    std::cout << "Client connected: socket=" << (int)clientSock << "\n";
    const int BUF_SIZE = 4096;
    char buffer[BUF_SIZE];
    std::string recvBuffer;

    while (true) {
        int bytes = recv(clientSock, buffer, BUF_SIZE, 0);
        if (bytes <= 0) {
            // disconnected or error
            std::cout << "Client disconnected: socket=" << (int)clientSock << "\n";
            removeClient(clientSock);
            closesocket(clientSock);
            return;
        }

        recvBuffer.append(buffer, buffer + bytes);

        // Our protocol: JSON messages delimited by '\n'. Process each full line.
        size_t pos;
        while ((pos = recvBuffer.find('\n')) != std::string::npos) {
            std::string line = recvBuffer.substr(0, pos);
            recvBuffer.erase(0, pos + 1);

            if (line.size() == 0) continue;
            try {
                auto [resp, shouldBroadcast] = processMessage(line);

                // send response to origin client (append newline)
                std::string out = resp + "\n";
                send(clientSock, out.c_str(), (int)out.size(), 0);

                // broadcast to others (exclude origin) for add/update
                if (shouldBroadcast) {
                    broadcast(out, clientSock);
                }
            }
            catch (const std::exception& ex) {
                std::cerr << "Error processing message: " << ex.what() << "\n";
            }
        }
    }
}

std::pair<std::string, bool> Server::processMessage(const std::string& msg) {
    json j = json::parse(msg);

    if (j.contains("action") && j["action"] == "get") {
        json arr = json::array();
        {
            std::lock_guard<std::mutex> lk(todoMu);
            for (auto& it : todoList) {
                arr.push_back({ {"id", it.id}, {"description", it.description}, {"status", it.completed ? "Completed" : "Pending"} });
            }
        }
        return { arr.dump(), false }; // don't broadcast
    }
    else if (j.contains("action") && j["action"] == "add") {
        std::string desc = j.value("description", std::string());
        TodoItem item;
        {
            std::lock_guard<std::mutex> lk(todoMu);
            item.id = nextId++;
            item.description = desc;
            item.completed = false;
            todoList.push_back(item);
        }
        json obj = { {"id", item.id}, {"description", item.description}, {"status", "Pending"} };
        std::cout << "Added item id=" << item.id << " desc=\"" << item.description << "\"\n";
        return { obj.dump(), true }; // broadcast to others
    }
    else if (j.contains("action") && j["action"] == "update") {
        int id = j.value("id", -1);
        TodoItem updated;
        bool found = false;
        {
            std::lock_guard<std::mutex> lk(todoMu);
            for (auto& it : todoList) {
                if (it.id == id) {
                    it.completed = !it.completed;
                    updated = it;
                    found = true;
                    break;
                }
            }
        }
        if (!found) {
            json err = { {"error", "item not found"} };
            return { err.dump(), false };
        }
        json obj = { {"id", updated.id}, {"description", updated.description}, {"status", updated.completed ? "Completed" : "Pending"} };
        std::cout << "Toggled item id=" << updated.id << " status=" << obj["status"] << "\n";
        return { obj.dump(), true };
    }
    else {
        json err = { {"error", "unknown action"} };
        return { err.dump(), false };
    }
}

void Server::broadcast(const std::string& message, SOCKET exclude) {
    std::lock_guard<std::mutex> lk(clientsMu);
    std::vector<SOCKET> failed;
    for (auto s : clients) {
        if (s == exclude) continue;
        int sent = send(s, message.c_str(), (int)message.size(), 0);
        if (sent == SOCKET_ERROR) {
            failed.push_back(s);
        }
    }
    for (auto s : failed) removeClient(s);
}

void Server::removeClient(SOCKET s) {
    std::lock_guard<std::mutex> lk(clientsMu);
    clients.erase(std::remove(clients.begin(), clients.end(), s), clients.end());
}

int main() {
    Server srv(5000);
    if (!srv.start()) {
        std::cerr << "Failed to start server.\n";
        return 1;
    }
    srv.runAcceptLoop();
    return 0;
}