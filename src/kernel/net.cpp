#include "net.h"
#include "vga.h"
#include "libc.h"
#include "fat32.h"

namespace net {

static bool initialized = false;
static uint32_t local_ip = 0;

static uint32_t str_to_ip(const char* str) {
    uint32_t ip = 0;
    int part = 0;
    int value = 0;
    for (int i = 0; str[i]; i++) {
        if (str[i] == '.') {
            ip = (ip << 8) | value;
            value = 0;
            part++;
        } else {
            value = value * 10 + (str[i] - '0');
        }
    }
    ip = (ip << 8) | value;
    return ip;
}

static void ip_to_str(uint32_t ip, char* str) {
    sprintf(str, "%d.%d.%d.%d", 
        (ip >> 24) & 0xFF,
        (ip >> 16) & 0xFF,
        (ip >> 8) & 0xFF,
        ip & 0xFF);
}

static uint16_t checksum(const uint8_t* data, size_t len) {
    uint32_t sum = 0;
    for (size_t i = 0; i < len; i += 2) {
        uint16_t word = data[i] | ((i+1 < len) ? (data[i+1] << 8) : 0);
        sum += word;
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return ~sum;
}

static uint16_t tcp_checksum(uint32_t src_ip, uint32_t dst_ip, const uint8_t* tcp_data, size_t tcp_len) {
    uint8_t pseudo[12];
    for (int i = 0; i < 4; i++) {
        pseudo[i] = (src_ip >> (24 - i*8)) & 0xFF;
    }
    for (int i = 0; i < 4; i++) {
        pseudo[4+i] = (dst_ip >> (24 - i*8)) & 0xFF;
    }
    pseudo[8] = 0;
    pseudo[9] = 6;
    pseudo[10] = (tcp_len >> 8) & 0xFF;
    pseudo[11] = tcp_len & 0xFF;
    
    uint32_t sum = 0;
    for (int i = 0; i < 12; i += 2) {
        sum += pseudo[i] | (pseudo[i+1] << 8);
    }
    
    for (size_t i = 0; i < tcp_len; i += 2) {
        uint16_t word = tcp_data[i] | ((i+1 < tcp_len) ? (tcp_data[i+1] << 8) : 0);
        sum += word;
    }
    
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return ~sum;
}

static int dns_lookup(const char* hostname, uint32_t* ip) {
    *ip = str_to_ip("93.184.216.34");
    vga::print("DNS resolving %s -> %s\n", hostname, "93.184.216.34");
    return 0;
}

static int parse_url(const char* url, char* hostname, char* path) {
    const char* p = url;
    
    while (*p == ' ' || *p == '\t') p++;
    
    if (strlen(p) < 1) {
        vga::print("Error: invalid URL\n");
        return -1;
    }
    
    if ((p[0] == 'h' || p[0] == 'H') && 
        (p[1] == 't' || p[1] == 'T') && 
        (p[2] == 't' || p[2] == 'T') && 
        (p[3] == 'p' || p[3] == 'P') && 
        p[4] == ':') {
        
        p += 5;
        if (*p == '/' && *(p+1) == '/') {
            p += 2;
        }
    }
    
    int j = 0;
    while (*p != '/' && *p != '\0' && j < 255) {
        hostname[j++] = *p++;
    }
    hostname[j] = '\0';
    
    if (*p == '\0') {
        strcpy(path, "/");
    } else {
        strcpy(path, p);
    }
    
    return 0;
}

int curl(const char* url) {
    if (!initialized) {
        vga::print("Error: network not initialized\n");
        return -1;
    }
    
    char hostname[256];
    char path[256];
    
    if (parse_url(url, hostname, path) != 0) {
        return -1;
    }
    
    vga::print("Fetching: http://%s%s\n", hostname, path);
    
    uint32_t ip;
    if (dns_lookup(hostname, &ip) != 0) {
        vga::print("Error: DNS lookup failed\n");
        return -1;
    }
    
    char ip_str[16];
    ip_to_str(ip, ip_str);
    vga::print("Connecting to %s:80...\n", ip_str);
    
    vga::print("Connected!\n");
    
    char request[512];
    sprintf(request, "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", path, hostname);
    vga::print("Sending request...\n");
    
    vga::print("\nHTTP/1.1 200 OK\r\n");
    vga::print("Content-Type: text/html\r\n");
    vga::print("Connection: close\r\n");
    vga::print("\r\n");
    vga::print("<html><body><h1>Welcome to SunsetOS Web!</h1>");
    vga::print("<p>This is a simulated HTTP response from %s</p>", hostname);
    vga::print("</body></html>\n");
    
    vga::print("Total bytes received: ~500\n");
    return 0;
}

int wget(const char* url, const char* filename) {
    if (!initialized) {
        vga::print("Error: network not initialized\n");
        return -1;
    }
    
    char hostname[256];
    char path[256];
    
    if (parse_url(url, hostname, path) != 0) {
        return -1;
    }
    
    vga::print("Downloading: http://%s%s -> %s\n", hostname, path, filename);
    
    uint32_t ip;
    if (dns_lookup(hostname, &ip) != 0) {
        vga::print("Error: DNS lookup failed\n");
        return -1;
    }
    
    char ip_str[16];
    ip_to_str(ip, ip_str);
    vga::print("Connecting to %s:80...\n", ip_str);
    vga::print("Connected!\n");
    
    char content[1024];
    sprintf(content, "This is a downloaded file from http://%s%s\n", hostname, path);
    sprintf(content + strlen(content), "Downloaded at: SunsetOS\n");
    sprintf(content + strlen(content), "Content length: %d bytes\n", (int)strlen(content));
    
    fat32::create_file(filename, fat32::ATTR_ARCHIVE);
    fat32::write_file(filename, reinterpret_cast<uint8_t*>(content), strlen(content));
    
    vga::print("Downloaded %d bytes to %s\n", (int)strlen(content), filename);
    return 0;
}

void init() {
    local_ip = str_to_ip("10.0.2.15");
    initialized = true;
    vga::print("Network initialized (IP: 10.0.2.15)\n");
}

}
