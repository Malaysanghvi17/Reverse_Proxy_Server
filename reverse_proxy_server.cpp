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


using namespace std;
class ReverseProxyServer {
public:
    ReverseProxyServer(const string& proxyAddress, int proxyPort, const vector<string>& targetAddresses)
        : proxyAddress(proxyAddress), proxyPort(proxyPort), targetAddresses(targetAddresses), running(true) {}

    void start() {
        listenSocket = createListeningSocket(proxyAddress, proxyPort);
        if (listenSocket == -1) {
            cerr << "Failed to create listening socket." << endl;
            return;
        }

        cout << "Proxy server listening on " << proxyAddress << ":" << proxyPort << endl;

        while (running) {
            int clientSocket = acceptConnection(listenSocket);
            if (clientSocket != -1) {
                thread(&ReverseProxyServer::handleConnection, this, clientSocket).detach();
            }
        }
    }

    void stop() {
        running = false;
    }

private:
    string proxyAddress;
    int proxyPort;
    vector<string> targetAddresses;
    int listenSocket;
    bool running;

    int createListeningSocket(const string& address, int port) {
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
            cerr << "Failed to accept connection." << endl;
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

        // cout << bytesRead << endl;
        // for(int i = 0; i < 4096; i++){
        //    cout << buffer[i];
        // }
        // cout << endl;

        string request(buffer, bytesRead);
        string targetAddress = getNextTargetAddress();

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

    string getNextTargetAddress() {
        static size_t currentIndex = 0;
        string target = targetAddresses[currentIndex];
        currentIndex = (currentIndex + 1) % targetAddresses.size();
        return target;
    }

    int connectToTarget(const string& targetAddress) {
        size_t pos = targetAddress.find(':');
        if (pos == string::npos) {
            cerr << "Invalid target address: " << targetAddress << endl;
            return -1;
        }

        string host = targetAddress.substr(0, pos);
        string portStr = targetAddress.substr(pos + 1);
        int port = stoi(portStr);

        int targetSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (targetSocket == -1) {
            cerr << "Failed to create target socket." << endl;
            return -1;
        }

        sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

        if (connect(targetSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
            cerr << "Failed to connect to target: " << targetAddress << endl;
            close(targetSocket);
            return -1;
        }

        return targetSocket;
    }
};

int main() {
    string proxyAddress = "127.0.0.1";
    int proxyPort = 8080;
    vector<string> targetAddresses = {"127.0.0.1:80", "127.0.0.1:8010"}; // Add your target servers here

    ReverseProxyServer proxyServer(proxyAddress, proxyPort, targetAddresses);
    proxyServer.start();

    return 0;
}
