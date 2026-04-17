#include <stdint.h>
#include "io.h"

#define RTL8139_VENDOR_ID 0x10EC
#define RTL8139_DEVICE_ID 0x8139

/* PCI Command Register bit to enable Bus Mastering (DMA) */
#define PCI_COMMAND_MASTER 0x0004 

/* RTL8139 Registers */
#define MAC_ADDR   0x00
#define TX_ADDR0   0x20
#define TX_STATUS0 0x10
#define RX_BUF     0x30
#define IMR        0x3C
#define ISR        0x3E
#define RCR        0x44
#define COMMAND    0x37
#define CONFIG1    0x52

extern void print(const char* str);
extern void itoa(int num, char* str, int base);

/* Memory buffers (Must be contiguous physical memory) */
uint8_t rx_buffer[8192 + 16 + 1500];
uint8_t tx_buffer[4][1536]; // 4 Transmit buffers
uint8_t tx_cur = 0;         // Current TX buffer

uint32_t net_io_addr = 0;
uint8_t my_mac[6];

/* Ethernet Frame Structure (Layer 2) */
typedef struct {
    uint8_t dest_mac[6];
    uint8_t src_mac[6];
    uint16_t ethertype; // e.g., 0x0800 for IPv4
    uint8_t payload[1500];
} __attribute__((packed)) ethernet_frame_t;

void net_init(uint32_t io_base) {
    net_io_addr = io_base;
    print("[NET] Realtek RTL8139 Initializing...\n");

    // 1. Turn on the card
    outb(net_io_addr + CONFIG1, 0x00);

    // 2. Software Reset
    outb(net_io_addr + COMMAND, 0x10);
    while((inb(net_io_addr + COMMAND) & 0x10) != 0); // Wait for reset to clear

    // 3. Read MAC Address
    print("MAC Address: ");
    for(int i=0; i<6; i++) {
        my_mac[i] = inb(net_io_addr + MAC_ADDR + i);
        char buf[3]; itoa(my_mac[i], buf, 16);
        print(buf); print(":");
    }
    print("\n");

    // 4. Setup Receive Buffer
    outl(net_io_addr + RX_BUF, (uint32_t)&rx_buffer);

    // 5. Setup Interrupts (Receive OK | Transmit OK)
    outw(net_io_addr + IMR, 0x0005);

    // 6. Configure Receive (Accept Broadcast (AB), My MAC (APM))
    outl(net_io_addr + RCR, 0x0F | (1 << 7)); 

    // 7. Enable TX and RX
    outb(net_io_addr + COMMAND, 0x0C); 

    print("[NET] Card Ready. Awaiting packets...\n");
}

/* Actually sends a raw frame out to QEMU's virtual network */
void net_send_raw_packet(uint8_t* dest_mac, uint16_t protocol, uint8_t* payload, uint32_t payload_len) {
    ethernet_frame_t* frame = (ethernet_frame_t*)tx_buffer[tx_cur];
    
    // Build Ethernet Header
    for(int i=0; i<6; i++) frame->dest_mac[i] = dest_mac[i];
    for(int i=0; i<6; i++) frame->src_mac[i] = my_mac[i];
    
    // Endianness swap for protocol (e.g. 0x0800 -> 0x0008)
    frame->ethertype = ((protocol & 0xFF) << 8) | ((protocol >> 8) & 0xFF); 
    
    // Copy payload
    for(uint32_t i=0; i<payload_len; i++) frame->payload[i] = payload[i];

    uint32_t total_len = payload_len + 14; // Payload + Header size
    if (total_len < 60) total_len = 60;    // Ethernet padding minimum

    // 1. Tell NIC where the buffer is in physical RAM
    outl(net_io_addr + TX_ADDR0 + (tx_cur * 4), (uint32_t)frame);
    
    // 2. Tell NIC the size and trigger the send
    outl(net_io_addr + TX_STATUS0 + (tx_cur * 4), total_len);

    print("[NET] Real packet transmitted!\n");
    
    // Cycle to next buffer
    tx_cur = (tx_cur + 1) % 4; 
}