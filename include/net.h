#ifndef NET_H
#define NET_H

#include <stdint.h>

void net_init(void);
void net_send_packet(const uint8_t* data, uint16_t len);
int net_receive_packet(uint8_t* buffer, uint16_t max_len);
void net_broadcast_hostname(void);
void net_set_hostname(const char* name);
const char* net_get_hostname(void);
void net_get_mac(uint8_t* mac_out);

#endif