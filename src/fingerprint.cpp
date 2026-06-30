#include "netnemesis.h"
#include <chrono>

std::string ServiceFingerprinter::grabBanner(const std::string &ip, int port, double timeout) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return "";
    
    struct timeval tv;
    tv.tv_sec = (int)timeout;
    tv.tv_usec = (int)((timeout - tv.tv_sec) * 1000000);
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
    
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return "";
    }
    
    // Query specifiche per gioco
    if (port == 25565) { // Minecraft Server List Ping
        // Handshake + Status Request
        unsigned char handshake[] = {
            0x10, // Packet length
            0x00, // Packet ID (Handshake)
            0x47, 0x00, // Protocol version (71)
            0x09, 0x6c, 0x6f, 0x63, 0x61, 0x6c, 0x68, 0x6f, 0x73, 0x74, // "localhost"
            0x63, 0xdd, // Port (25565)
            0x01, // Next state (Status)
            0x01, // Length
            0x00  // Status Request
        };
        send(sock, handshake, sizeof(handshake), 0);
    } else if (port >= 27015 && port <= 27020) { // Source Engine
        // A2S_INFO query
        char a2s_info[] = "\xFF\xFF\xFF\xFF\x54Source Engine Query\0";
        send(sock, a2s_info, sizeof(a2s_info), 0);
    } else {
        // Banner generico
        send(sock, "\r\n", 2, 0);
    }
    
    char buffer[2048] = {0};
    int received = recv(sock, buffer, sizeof(buffer) - 1, 0);
    close(sock);
    
    if (received > 0) {
        return std::string(buffer, received);
    }
    return "";
}

ServiceFingerprinter::ServiceInfo ServiceFingerprinter::parseMinecraftResponse(const std::string &data) {
    ServiceInfo info;
    info.name = "Minecraft";
    
    // Parsing JSON risposta Minecraft
    size_t version_pos = data.find("\"version\":");
    if (version_pos != std::string::npos) {
        size_t start = data.find("\"", version_pos + 10);
        size_t end = data.find("\"", start + 1);
        if (start != std::string::npos && end != std::string::npos) {
            info.version = data.substr(start + 1, end - start - 1);
        }
    }
    
    return info;
}

ServiceFingerprinter::ServiceInfo ServiceFingerprinter::parseSourceEngineResponse(const std::string &data) {
    ServiceInfo info;
    info.name = "Source Engine";
    
    if (data.size() > 4 && data[4] == 'I') {
        size_t idx = 5;
        // Skip header
        while (idx < data.size() && data[idx] != '\0') idx++;
        idx++; // Null terminator
        
        // Server name
        std::string server_name;
        while (idx < data.size() && data[idx] != '\0') {
            server_name += data[idx++];
        }
        info.extra_info = server_name;
    }
    
    return info;
}

ServiceFingerprinter::ServiceInfo ServiceFingerprinter::fingerprint(const std::string &ip, int port) {
    ServiceInfo info;
    info.name = "Unknown";
    info.version = "Unknown";
    
    std::string banner = grabBanner(ip, port);
    if (banner.empty()) return info;
    
    // Identificazione basata su porta e risposta
    if (port == 25565) {
        info = parseMinecraftResponse(banner);
    } else if (port >= 27015 && port <= 27020) {
        info = parseSourceEngineResponse(banner);
        info.name = "Source Engine (CS:GO/TF2)";
    } else if (banner.find("HTTP") != std::string::npos) {
        info.name = "HTTP Server";
        info.version = "Web Service";
    } else if (banner.find("SSH") != std::string::npos) {
        info.name = "SSH";
        size_t ver_pos = banner.find("SSH-");
        if (ver_pos != std::string::npos) {
            info.version = banner.substr(ver_pos, banner.find(' ', ver_pos) - ver_pos);
        }
    }
    
    return info;
}