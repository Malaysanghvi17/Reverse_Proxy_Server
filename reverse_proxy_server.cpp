#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <iterator>
#include <algorithm>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cassert>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

class ReverseProxyServer {
public:
    ReverseProxyServer(const std::string& proxyAddress, int proxyPort, const std::vector<std::string>& targetAddresses)
        : proxyAddress_(proxyAddress), proxyPort_(proxyPort), targetAddresses_(targetAddresses), running_(true) {}

    void start() {
        listenSocket_ = createListeningSocket(proxyAddress_, proxyPort_);
        if (listenSocket_ == -1) {
            std::cerr << "Failed to create listening socket." << std::endl;
            return;
        }

        std::cout << "Proxy server listening on " << proxyAddress_ << ":" << proxyPort_ << std::endl;

        while (running_) {
            int clientSocket = acceptConnection(listenSocket_);
            if (clientSocket != -1) {
                std::thread(&ReverseProxyServer::handleConnection, this, clientSocket).detach();
            }
        }
    }

    void stop() {
        running_ = false;
    }

private:
    std::string proxyAddress_;
    int proxyPort_;
    std::vector<std::string> targetAddresses_;
    int listenSocket_;
    bool running_;

    int createListeningSocket(const std::string& address, int port) {
        int listenSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (listenSocket == -1) {
            return -1;
        }

        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, address.c_str(), &addr.sin_addr);

        if (bind(listenSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
            close(listenSocket);
            return -1;
        }

        if (listen(listenSocket, SOMAXCONN) == -1) {
            close(listenSocket);
            return -1;
        }

        return listenSocket;
    }

    int acceptConnection(int listenSocket) {
        sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        int clientSocket = accept(listenSocket, reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrLen);
        if (clientSocket == -1) {
            std::cerr << "Failed to accept connection." << std::endl;
            return -1;
        }
        return clientSocket;
    }

    void handleConnection(int clientSocket) {
        char buffer[4096];
        int bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
        if (bytesRead <= 0) {
            close(clientSocket);
            return;
        }

        cout << bytesRead << endl;
        for(int i = 0; i < 4096; i++){
            cout << buffer[i];
        }
        cout << endl;
        
        std::string request(buffer, bytesRead);
        std::string targetAddress = getNextTargetAddress();

        int targetSocket = connectToTarget(targetAddress);
        if (targetSocket == -1) {
            close(clientSocket);
            return;
        }

        send(targetSocket, request.c_str(), request.length(), 0);

        // Forward response from target server back to client
        while ((bytesRead = recv(targetSocket, buffer, sizeof(buffer), 0)) > 0) {
            send(clientSocket, buffer, bytesRead, 0);
        }

        close(targetSocket);
        close(clientSocket);
    }

    std::string getNextTargetAddress() {
        static std::size_t currentIndex = 0;
        std::string target = targetAddresses_[currentIndex];
        currentIndex = (currentIndex + 1) % targetAddresses_.size();
        return target;
    }

    int connectToTarget(const std::string& targetAddress) {
        size_t pos = targetAddress.find(':');
        if (pos == std::string::npos) {
            std::cerr << "Invalid target address: " << targetAddress << std::endl;
            return -1;
        }

        std::string host = targetAddress.substr(0, pos);
        std::string portStr = targetAddress.substr(pos + 1);
        int port = std::stoi(portStr);

        int targetSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (targetSocket == -1) {
            std::cerr << "Failed to create target socket." << std::endl;
            return -1;
        }

        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

        if (connect(targetSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
            std::cerr << "Failed to connect to target: " << targetAddress << std::endl;
            close(targetSocket);
            return -1;
        }

        return targetSocket;
    }
};

int main() {
    std::string proxyAddress = "127.0.0.1";
    int proxyPort = 8080;
    std::vector<std::string> targetAddresses = {"127.0.0.1:8000", "127.0.0.1:8001"}; // Add target servers here

    ReverseProxyServer proxyServer(proxyAddress, proxyPort, targetAddresses);
    proxyServer.start();

    return 0;
}
