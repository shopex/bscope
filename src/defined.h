#define __FAVOR_BSD
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include NIDS_H
#include <pcap.h>
#include <amqp_tcp_socket.h>
#include <amqp.h>
#include <amqp_framing.h>
#include <msgpack.h>

#ifdef debug
#define print_data(data, len) print_data_std(data, len)
#else
#define print_data(data, len) ;
#endif

#ifdef debug_line
#define dline() printf("dline() = %s:%d\n" ,__FILE__, __LINE__)
#else
#define dline() ;
#endif

struct amqp_master{
    char *host;
    char *user;
    char *password;
    char *vhost;
    char *exchange;
    int port;
    char connected;
    amqp_socket_t *socket;
    amqp_connection_state_t conn;
};

typedef struct {
    const char* pid_file;
    const char* config_file;
    const char* netif;
    const char* node;
    FILE* * swap_fd;
    FILE* log_fd;
    char skip_checksum;
    char daemon_mode;
    char supervisor_mode;
    struct amqp_master master;
    char *hostname[255];
    unsigned short hostname_len;
    unsigned short hostport[255];
    unsigned short hostport_len;
    unsigned int hostip[255][2];
    unsigned short hostip_len;
    unsigned int catch_code[1000];

    char *catch_req_head[255];
    unsigned short catch_req_head_len;

    char *catch_rsp_head[255];
    unsigned short catch_rsp_head_len;

    char *catch_cookie[255];
    unsigned short catch_cookie_len;

    char *catch_get[255];
    unsigned short catch_get_len;

    char *catch_post[255];
    unsigned short catch_post_len;
} configuration;

typedef struct time_v_pair{
    time_t time;
    unsigned int value;
} time_v_pair;

#define RPS_N 600

typedef struct status{
    pid_t pid;
    unsigned int conn;
    struct time_v_pair rps[RPS_N];
    unsigned int p;
    char alive;
    char life;
    char data;
} status;

struct msg_report {
    msgpack_sbuffer* buffer;
    msgpack_packer* pk;
    char * routing_key;
    short count;
};

status * stat;
configuration config;

#define LOG_BUF_SIZE 8192

