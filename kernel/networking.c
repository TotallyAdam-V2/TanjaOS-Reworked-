#include "../include/net.h"

#define E1000_REG_CTRL    0x00000
#define E1000_REG_STATUS  0x00008
#define E1000_REG_ICR     0x000C0
#define E1000_REG_IMS     0x000D0
#define E1000_REG_RCTL    0x00100
#define E1000_REG_TCTL    0x00400
#define E1000_REG_RAL     0x05400
#define E1000_REG_RAH     0x05404
#define E1000_REG_TDBAL   0x03800
#define E1000_REG_TDBAH   0x03804
#define E1000_REG_TDLEN   0x03808
#define E1000_REG_TDH     0x03810
#define E1000_REG_TDT     0x03818
#define E1000_REG_RDBAL   0x02800
#define E1000_REG_RDBAH   0x02804
#define E1000_REG_RDLEN   0x02808
#define E1000_REG_RDH     0x02810
#define E1000_REG_RDT     0x02818

struct e1000_tx_desc {
    uint64_t addr;
    uint16_t length;
    uint8_t cso;
    uint8_t cmd;
    uint8_t status;
    uint8_t css;
    uint16_t special;
} __attribute__((packed));

struct e1000_rx_desc {
    uint64_t addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t status;
    uint8_t errors;
    uint16_t special;
} __attribute__((packed));

static uint32_t e1000_mmio_base = 0;
static char net_hostname[64] = "tanjaos";
static uint8_t net_mac[6];
static struct e1000_tx_desc tx_descs[8] __attribute__((aligned(16)));
static uint8_t tx_buffers[8][1536] __attribute__((aligned(16)));
static struct e1000_rx_desc rx_descs[8] __attribute__((aligned(16)));
static uint8_t rx_buffers[8][1536] __attribute__((aligned(16)));
static uint32_t rx_cur = 0;

static void e1000_write_reg(uint32_t offset, uint32_t val) {
    if (e1000_mmio_base) {
        *(volatile uint32_t*)(e1000_mmio_base + offset) = val;
    }
}

static uint32_t e1000_read_reg(uint32_t offset) {
    if (e1000_mmio_base) {
        return *(volatile uint32_t*)(e1000_mmio_base + offset);
    }
    return 0;
}

void net_init(void) {
    e1000_mmio_base = 0xFEB80000;

    uint32_t status = e1000_read_reg(E1000_REG_STATUS);
    if (status == 0xFFFFFFFF) {
        e1000_mmio_base = 0;
        return;
    }

    // Issue a global device reset
    uint32_t ctrl = e1000_read_reg(E1000_REG_CTRL);
    e1000_write_reg(E1000_REG_CTRL, ctrl | (1 << 26));
    
    // Wait briefly for reset to complete
    for (volatile int i = 0; i < 100000; i++);

    // Re-read control and set Link Up (SLU)
    ctrl = e1000_read_reg(E1000_REG_CTRL);
    e1000_write_reg(E1000_REG_CTRL, ctrl | (1 << 6) | (1 << 26));

    // Read MAC Address from Receive Address Low/High registers
    uint32_t ral = e1000_read_reg(E1000_REG_RAL);
    uint32_t rah = e1000_read_reg(E1000_REG_RAH);

    net_mac[0] = (ral >> 0) & 0xFF;
    net_mac[1] = (ral >> 8) & 0xFF;
    net_mac[2] = (ral >> 16) & 0xFF;
    net_mac[3] = (ral >> 24) & 0xFF;
    net_mac[4] = (rah >> 0) & 0xFF;
    net_mac[5] = (rah >> 8) & 0xFF;

    // Initialize Transmit Descriptors
    for (int i = 0; i < 8; i++) {
        tx_descs[i].addr = (uint64_t)(uint32_t)tx_buffers[i];
        tx_descs[i].length = 0;
        tx_descs[i].cmd = 0;
        tx_descs[i].status = 1;
        tx_descs[i].cso = 0;
        tx_descs[i].css = 0;
        tx_descs[i].special = 0;
    }

    e1000_write_reg(E1000_REG_TDBAL, (uint32_t)tx_descs);
    e1000_write_reg(E1000_REG_TDBAH, 0);
    e1000_write_reg(E1000_REG_TDLEN, sizeof(tx_descs));
    e1000_write_reg(E1000_REG_TDH, 0);
    e1000_write_reg(E1000_REG_TDT, 0);

    // Initialize Receive Descriptors
    for (int i = 0; i < 8; i++) {
        rx_descs[i].addr = (uint64_t)(uint32_t)rx_buffers[i];
        rx_descs[i].length = 0;
        rx_descs[i].status = 0;
        rx_descs[i].errors = 0;
    }

    e1000_write_reg(E1000_REG_RDBAL, (uint32_t)rx_descs);
    e1000_write_reg(E1000_REG_RDBAH, 0);
    e1000_write_reg(E1000_REG_RDLEN, sizeof(rx_descs));
    e1000_write_reg(E1000_REG_RDH, 0);
    e1000_write_reg(E1000_REG_RDT, 7);

    // Disable interrupts or set IMS mask
    e1000_write_reg(E1000_REG_IMS, 0x1F6DC);

    // Configure Receive Control (RCTL):
    // Bit 1: Receiver Enable (RE)
    // Bit 2: Store Bad Packets (SBP) -> set to 0 or 1 depending on preference, usually 0
    // Bit 3: Broadcast Accept Mode (BAM) -> crucial for receiving broadcast/gateway traffic
    // Bit 4-5: Descriptor Minimum Threshold / Buffer size (Clear for 2048 bytes or set BSIZE)
    // Bit 15: Loopback Mode
    // Bit 26: Strip CRC (SECR) -> 1 to strip ethernet checksum automatically
    uint32_t rctl = (1 << 1)   // Receiver Enable
                 | (1 << 2)   // Multicast Promiscuous (optional, helps catch traffic)
                 | (1 << 3)   // Broadcast Accept Mode (BAM) - CRITICAL
                 | (1 << 15)  // Unicast Promiscuous (UPE) - ensures we catch packets sent to our MAC
                 | (1 << 26); // Strip Ethernet CRC
    e1000_write_reg(E1000_REG_RCTL, rctl);

    // Configure Transmit Control (TCTL):
    // Bit 1: Transmitter Enable (EN)
    // Bit 3: Pad Short Packets (PSP)
    // Bits 4-11: Collision Threshold
    uint32_t tctl = (1 << 1)   // Transmit Enable
                 | (1 << 3)   // Pad Short Packets
                 | (1 << 10); // Collision Threshold
    e1000_write_reg(E1000_REG_TCTL, tctl);
}

void net_send_packet(const uint8_t* data, uint16_t len) {
    if (!e1000_mmio_base || len > 1536) return;

    uint32_t tail = e1000_read_reg(E1000_REG_TDT);
    for (uint16_t i = 0; i < len; i++) {
        tx_buffers[tail][i] = data[i];
    }

    tx_descs[tail].length = len;
    tx_descs[tail].cmd = (1 << 0) | (1 << 3); 
    tx_descs[tail].status = 0;

    tail = (tail + 1) % 8;
    e1000_write_reg(E1000_REG_TDT, tail);
}

int net_receive_packet(uint8_t* buffer, uint16_t max_len) {
    if (!e1000_mmio_base) return 0;

    // Check if current descriptor has been written to by hardware (DD bit set)
    if (!(rx_descs[rx_cur].status & 0x01)) {
        return 0; 
    }

    uint16_t len = rx_descs[rx_cur].length;
    if (len > max_len) len = max_len;

    for (uint16_t i = 0; i < len; i++) {
        buffer[i] = rx_buffers[rx_cur][i];
    }

    // Keep track of the descriptor index we just consumed
    uint32_t old_rx_cur = rx_cur;

    // Reset descriptor status and advance our software ring pointer
    rx_descs[rx_cur].status = 0;
    rx_cur = (rx_cur + 1) % 8; // Assuming ring size of 8 descriptors

    // CRITICAL: Tell the E1000 hardware that this descriptor buffer is free again
    e1000_write_reg(E1000_REG_RDT, old_rx_cur);

    return len;
}

void net_broadcast_hostname(void) {
    uint8_t packet[1500];
    for (int i = 0; i < 6; i++) {
        packet[0 + i] = 0xFF; 
        packet[6 + i] = net_mac[i]; 
    }
    packet[12] = 0x88;
    packet[13] = 0xB5; 

    int len = 14;
    int i = 0;
    while (net_hostname[i] && len < 1499) {
        packet[len++] = net_hostname[i++];
    }
    packet[len] = 0;

    net_send_packet(packet, len + 1);
}

void net_set_hostname(const char* name) {
    if (!name) return;
    int i = 0;
    while (name[i] && i < 63) {
        net_hostname[i] = name[i];
        i++;
    }
    net_hostname[i] = 0;
    net_broadcast_hostname();
}

const char* net_get_hostname(void) {
    return net_hostname;
}

void net_get_mac(uint8_t* mac_out) {
    for (int i = 0; i < 6; i++) {
        mac_out[i] = net_mac[i];
    }
}