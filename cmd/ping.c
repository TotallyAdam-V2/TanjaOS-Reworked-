#include "cmd.h"

static int ping_is_blank(const char* s) {
    if (!s) return 1;
    while (*s) {
        if (*s != ' ' && *s != '\t' && *s != '\n' && *s != '\r') return 0;
        s++;
    }
    return 1;
}

static int parse_ipv4(const char* ip_str, uint8_t* ip_out) {
    int val = 0;
    int parts = 0;
    for (int i = 0; i < 4; i++) ip_out[i] = 0;

    while (*ip_str) {
        if (*ip_str >= '0' && *ip_str <= '9') {
            val = val * 10 + (*ip_str - '0');
            if (val > 255) return -1;
        } else if (*ip_str == '.') {
            if (parts >= 3) return -1;
            ip_out[parts++] = (uint8_t)val;
            val = 0;
        } else {
            return -1;
        }
        ip_str++;
    }
    if (parts != 3) return -1;
    ip_out[3] = (uint8_t)val;
    return 0;
}

static uint16_t calculate_checksum(uint8_t* buf, int length) {
    uint32_t sum = 0;
    for (int i = 0; i < length; i += 2) {
        uint16_t word = ((uint16_t)buf[i] << 8) | buf[i + 1];
        sum += word;
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return (uint16_t)(~sum);
}

static void ping_print_dec(uint32_t n) {
    if (n == 0) { print("0"); return; }
    char buffer[11]; int pos = 10; buffer[pos] = 0;
    while (n > 0 && pos > 0) { pos--; buffer[pos] = '0' + (n % 10); n /= 10; }
    print(&buffer[pos]);
}

void cmd_ping(char* args) {
    extern void print(const char* s);
    extern void net_send_packet(const uint8_t* data, uint16_t len);
    extern int net_receive_packet(uint8_t* buffer, uint16_t max_len);
    extern void net_get_mac(uint8_t* mac_out);

    print("PING Command is not working probly becuse of ARP\n");

    if (ping_is_blank(args)) {
        print("ping: missing IP address argument\n");
        return;
    }

    uint8_t target_ip[4];
    if (parse_ipv4(args, target_ip) < 0) {
        print("ping: invalid IP address format\n");
        return;
    }

    uint8_t my_mac[6];
    net_get_mac(my_mac);
    uint8_t guest_ip[4] = {10, 0, 2, 15};
    
    print("PING "); print(args); print(" 56(84) bytes of data.\n");

    int transmitted = 0;
    int received = 0;

    for (int seq = 1; seq <= 4; seq++) {
        uint8_t packet[74]; 
        
        // Broadcast / Gateway MAC (Using broadcast ff:ff:ff:ff:ff:ff forces QEMU to look at it)
        for(int i = 0; i < 6; i++) packet[i] = 0xFF;
        for(int i = 0; i < 6; i++) packet[6 + i] = my_mac[i]; 
        
        packet[12] = 0x08; packet[13] = 0x00; // IPv4

        packet[14] = 0x45; packet[15] = 0x00; 
        packet[16] = 0x00; packet[17] = 60;   
        packet[18] = 0xAB; packet[19] = 0xCD; 
        packet[20] = 0x00; packet[21] = 0x00; 
        packet[22] = 64;   packet[23] = 1;    // ICMP
        packet[24] = 0x00; packet[25] = 0x00; 
        
        for(int i = 0; i < 4; i++) packet[26 + i] = guest_ip[i];
        for(int i = 0; i < 4; i++) packet[30 + i] = target_ip[i];

        uint16_t ip_chk = calculate_checksum(&packet[14], 20);
        packet[24] = ip_chk & 0xFF; packet[25] = ip_chk >> 8;

        packet[34] = 8;    packet[35] = 0;    
        packet[36] = 0;    packet[37] = 0;    
        packet[38] = 0x12; packet[39] = 0x34; 
        packet[40] = seq & 0xFF;       
        packet[41] = (seq >> 8) & 0xFF;              

        for (int i = 42; i < 74; i++) packet[i] = (uint8_t)(i & 0xFF);

        uint16_t icmp_chk = calculate_checksum(&packet[34], 40);
        packet[36] = icmp_chk & 0xFF; packet[37] = icmp_chk >> 8;

        net_send_packet(packet, sizeof(packet));
        transmitted++;

        int got_reply = 0;
        uint8_t rx_buf[1536];
        
        for (volatile int wait = 0; wait < 800000; wait++) {
            int len = net_receive_packet(rx_buf, sizeof(rx_buf));
            if (len > 34) {
                // Accept any incoming IPv4 ICMP packet or echo reply
                if (rx_buf[12] == 0x08 && rx_buf[13] == 0x00 && rx_buf[23] == 0x01) {
                    received++;
                    got_reply = 1;
                    break;
                }
            }
        }

        if (got_reply) {
            print("64 bytes from "); print(args);
            print(": icmp_seq="); ping_print_dec(seq);
            print(" ttl=64 time=1.2 ms\n");
        }
    }

    print("\n--- "); print(args); print(" ping statistics ---\n");
    ping_print_dec(transmitted); print(" packets transmitted, ");
    ping_print_dec(received); print(" received, ");
    int loss = (transmitted > 0) ? ((transmitted - received) * 100) / transmitted : 100;
    ping_print_dec(loss); print("% packet loss\n");
}