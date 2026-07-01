#include "netnemesis.h"
#include <sstream>
#include <sys/ioctl.h>
#include <net/if.h>

void Utils::logInfo(const std::string &msg) {
    std::lock_guard<std::mutex> lock(g_print_mutex);
    std::cout << "[\033[36mINFO\033[0m] " << msg << std::endl;
}

void Utils::logSuccess(const std::string &msg) {
    std::lock_guard<std::mutex> lock(g_print_mutex);
    std::cout << "[\033[32mSUCCESS\033[0m] " << msg << std::endl;
}

void Utils::logError(const std::string &msg) {
    std::lock_guard<std::mutex> lock(g_print_mutex);
    std::cerr << "[\033[31mERROR\033[0m] " << msg << std::endl;
}

void Utils::logWarning(const std::string &msg) {
    std::lock_guard<std::mutex> lock(g_print_mutex);
    std::cout << "[\033[33mWARNING\033[0m] " << msg << std::endl;
}

void Utils::logAttack(const std::string &msg) {
    std::lock_guard<std::mutex> lock(g_print_mutex);
    std::cout << "[\033[35mATTACK\033[0m] " << msg << std::endl;
}

void Utils::clearScreen() {
    system("clear");
}

std::vector<std::string> Utils::split(const std::string &s, char delim) {
    std::vector<std::string> tokens;
    std::stringstream ss(s);
    std::string token;
    while (getline(ss, token, delim)) {
        tokens.push_back(token);
    }
    return tokens;
}

void Utils::animateAttack(const std::string &type, const std::string &target) {
    std::vector<std::string> frames = {
        "[    ] Inizializzazione...",
        "[=>  ] Caricamento payload...",
        "[==> ] Connessione...",
        "[===>] Invio pacchetti...",
        "[████] ATTACCO IN CORSO"
    };
    
    for (const auto &frame : frames) {
        {
            std::lock_guard<std::mutex> lock(g_print_mutex);
            std::cout << "\r\033[31m" << frame << "\033[0m" << std::flush;
        }
        usleep(300000);
    }
    std::cout << std::endl;
    logAttack(type + " su " + target);
}

std::string Utils::getMacAddress(const std::string &iface) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    struct ifreq ifr;
    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name, iface.c_str(), IFNAMSIZ-1);
    ioctl(sock, SIOCGIFHWADDR, &ifr);
    close(sock);
    
    unsigned char *mac = (unsigned char *)ifr.ifr_hwaddr.sa_data;
    char mac_str[18];
    sprintf(mac_str, "%02x:%02x:%02x:%02x:%02x:%02x",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return std::string(mac_str);
}