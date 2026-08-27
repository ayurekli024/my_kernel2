#ifndef RTL8139_H
#define RTL8139_H

#define TCP_CLOSED 0
#define TCP_SYN_SENT 1
#define TCP_ESTABLISHED 2

// Olay Güdümlü TCP Soket Mimarisi
typedef struct {
    int active;
    int state;
    unsigned char remote_ip[4];
    unsigned short remote_port;
    unsigned short local_port;
    unsigned int seq;
    unsigned int ack;
    
    unsigned char rx_buf[8192];
    int rx_size;
    volatile int rx_ready;
} tcp_socket_t;

void init_rtl8139(void);

// Yeni Çekirdek İçi Ağ API'leri
int net_socket_create(void);
int net_tcp_connect(int sock_id, unsigned char* ip, unsigned short port);
int net_tcp_send(int sock_id, unsigned char flags, unsigned char* data, int len);
int net_tcp_recv(int sock_id, unsigned char* buf, int max_len);

#endif