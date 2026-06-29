#include "netnemesis.h"

void NetworkScanner::start() {
    scan_thread = std::thread(&NetworkScanner::scanLoop, this);
    Utils::logInfo("Scanner passivo avviato (intervallo: 2 minuti)");
}

bool NetworkScanner::probePort(const std::string &ip, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return false;
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
    
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    int result = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    close(sock);
    return (result == 0);
}

void NetworkScanner::scanLoop() {
    std::vector<int> game_ports = {27015, 27016, 25565, 7777, 7778, 27960, 28960, 64738, 27005};
    
    while (g_running) {
        Utils::logInfo("Inizio scansione rete locale...");
        
        // Scansione 192.168.1.0/24
        for (int host = 1; host <= 254; host++) {
            std::string ip = "192.168.1." + std::to_string(host);
            
            for (int port : game_ports) {
                if (probePort(ip, port)) {
                    std::lock_guard<std::mutex> lock(servers_mutex);
                    discovered_servers.push_back({ip, port});
                    Utils::logSuccess("Server di gioco trovato: " + ip + ":" + std::to_string(port));
                }
            }
        }
        
        // Attendi 2 minuti
        for (int i = 0; i < 120 && g_running; i++) {
            sleep(1);
        }
    }
}

void NetworkScanner::stop() {
    if (scan_thread.joinable()) scan_thread.join();
}

std::vector<std::pair<std::string, int>> NetworkScanner::getServers() {
    std::lock_guard<std::mutex> lock(servers_mutex);
    return discovered_servers;
}