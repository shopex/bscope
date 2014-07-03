#include <time.h>
#include "http-parser/http_parser.h"

#define CONST_STR_LEN(s) s, sizeof(s) - 1

#ifdef debug
#define debug(arg...) printf("<%x>: ", s);printf(arg);printf("\n");
#else
#define debug(...) ;
#endif
#define int_ntoa(x) inet_ntoa(*((struct in_addr *)&x))

struct kv{
    char * key;
    char * value;
    char type;
    struct kv * next;
};

struct body_buf{
    char *data;
    unsigned short size;
    struct body_buf *next;
};

#define POST_BUF_LENGTH 4096

typedef struct session {
    struct tcp_stream * tcp;
    struct http_parser *client_parser;
    struct http_parser *server_parser;

    //---------------------------------------------------
    unsigned char method;  //之前的属性是不会被重置的
    unsigned char need_report;
    unsigned char catched_cookie;
    unsigned char is_catch_response_body;
    unsigned char request_multipart;
    unsigned char cur_field;
    unsigned short status_code;
    unsigned short http_major;
    unsigned short http_minor;
    unsigned short response_body_size;
    const char * path;
    const char * host;
    const char * cookie;
    const char * referer;
    const char * ua;
    const char * last_request_header;
    const char * last_response_header;
    const char * post_buf;
    unsigned short post_buf_len;
    long req_time;
    long server_time;
    long rep_time;
    time_t start_time;
    struct kv * proplist;
    struct timeval status_time;
    struct body_buf *response_body_first;
    struct body_buf *response_body_last;
};


http_parser_settings client_parser_settings;
http_parser_settings server_parser_settings;

#define T_REQUEST_HEADER 1
#define T_RESPONSE_HEADER 2
#define T_COOKIE 3
#define T_GET 4
#define T_POST 5
