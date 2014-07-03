//#define debug
//#define debug_line
#include <string.h>
#include <stdlib.h>

#include "defined.h"
#include "session.h"

#define HTTP_WAIT_CLIENT 0
#define HTTP_REQ_HDR 1
#define HTTP_REQ_BODY 2
#define HTTP_WAIT_SERVER 3
#define HTTP_RSP_HDR 4
#define HTTP_RSP_BODY 5
#define HTTP_RSP_FINISH 6

#define JOIN_BUFF_SIZE 128
#define ROUTING_KEY_BUF_SIZE 512
char join_buff[JOIN_BUFF_SIZE];
char * join(char * left, char * right){
    strcpy(&join_buff, left);
    strncat(&join_buff, right, JOIN_BUFF_SIZE);
    return &join_buff;
}

char * kv_type_string(char type, char * key){
    if(type==T_REQUEST_HEADER){
        return join("req-", key);
    }else if(type==T_RESPONSE_HEADER){
        return join("header-", key);
    }else if(type==T_COOKIE){
        return join("cookie-", key);
    }else if(type==T_GET){
        return join("get-", key);
    }else if(type==T_POST){
        return join("post-", key);
    }
}

void free_prop(struct kv *h){
    if(h!=0){
        free_item(&h->key);
        free_item(&h->value);
        struct kv * next = h->next;
        free(h);
        free_prop(next);
    }
}

int free_item(void ** p){
    if(*p!=0){
        free(*p);
        *p = NULL;
    }
}

void free_buf(struct body_buf * b){
    if(b!=0){
        free(b->data);
        struct body_buf * next = b->next;
        free(b);
        free_buf(next);
    }
}

void session_init(struct session *s){
    bzero(&s->method, sizeof(struct session) - ((char *)&s->method - (char *)s));
}

void session_start(struct session *s){
    debug("session_start");
    http_parser_init(s->client_parser, HTTP_REQUEST);
    http_parser_init(s->server_parser, HTTP_RESPONSE);
    s->client_parser->data = s;
    s->server_parser->data = s;
    session_init(s);
}

void session_clean(struct session *s){
    debug("session_clean");
    free_item(&s->path);
    free_item(&s->host);
    free_item(&s->cookie);
    free_item(&s->referer);
    free_item(&s->ua);
    free_item(&s->post_buf);
    free_item(&s->last_request_header);
    free_item(&s->last_response_header);
    free_prop(s->proplist);
    s->proplist = NULL;
    free_buf(s->response_body_first);
    s->response_body_first = NULL;
}

char * close_status(char type){
    if(type==0){
        return "success";
    }else if(type==NIDS_TIMED_OUT){
        return "tcp_timeout";
    }else if(type==NIDS_CLOSE){
        return "tcp_closed";
    }else if(type==NIDS_RESET){
        return "tcp_reset";
    }
}

char log_buff[LOG_BUF_SIZE];
void session_log(struct session * s){
    char timestr[80];
    char *clientip = strdup(int_ntoa(s->tcp->addr.saddr));
    struct tm *newtime;
    newtime = localtime(&(s->start_time));
    strftime(&timestr, 80, "%d/%b/%Y:%H:%M:%S %z", newtime);
    snprintf(&log_buff, LOG_BUF_SIZE, "%s %s:%d %s - [%s] \"%s %s HTTP/%d.%d\" %d %d \"%s\" \"%s\""
            , clientip
            , int_ntoa(s->tcp->addr.daddr)
            , s->tcp->addr.dest
            , s->host
            , &timestr
            , http_method_str(s->method)
            , s->path
            , s->http_major
            , s->http_minor
            , s->status_code
            , s->tcp->client.count
            , s->referer?s->referer:""
            , s->ua?s->ua:""
        );
    free(clientip);
    status_logpush(&log_buff, strnlen(&log_buff, LOG_BUF_SIZE));
}

void report_add_key(struct msg_report* rp, char * key){
    int len = strlen(key);
    msgpack_pack_raw(rp->pk, len);
    msgpack_pack_raw_body(rp->pk, key, len);
    rp->count++;
}

void report_add_pair(struct msg_report* rp, char * key, char * value){
    if(!key) return;
    report_add_key(rp, key);

    if(value){
        int len = strlen(value);
        msgpack_pack_raw(rp->pk, len);
        msgpack_pack_raw_body(rp->pk, value, len);
    }else{
        msgpack_pack_nil(rp->pk);
    }
}

void report_add_pair_int(struct msg_report* rp, char * key, int value){
    if(!key) return;
    report_add_key(rp, key);
    msgpack_pack_int(rp->pk, value);
}

void report_add_pair_float(struct msg_report* rp, char * key, int value){
    if(!key) return;
    report_add_key(rp, key);
    msgpack_pack_float(rp->pk, value);
}

void session_report(struct session * s, char type){
    if(s->need_report==0){
        return;
    }
    s->need_report = 0;

    struct msg_report rp;
    rp.buffer = msgpack_sbuffer_new();
    rp.pk = msgpack_packer_new(rp.buffer, msgpack_sbuffer_write);
    rp.count = 0;

// 15 < keys < 65535
// https://github.com/msgpack/msgpack/blob/master/spec.md#formats-map
// +--------+--------+--------+~~~~~~~~~~~~~~~~~+
// |  0xde  |YYYYYYYY|YYYYYYYY|   N*2 objects   |
// +--------+--------+--------+~~~~~~~~~~~~~~~~~+
    msgpack_pack_map(rp.pk, 20);
    report_add_pair(&rp, "@class", "http-scope");
    report_add_pair_int(&rp, "@time", s->start_time);
    report_add_pair(&rp, "method", http_method_str(s->method));
    report_add_pair(&rp, "host", s->host);
    report_add_pair(&rp, "host", s->host);
    report_add_pair(&rp, "path", s->path);
    report_add_pair(&rp, "node", config.node);
    report_add_pair_int(&rp, "code", s->status_code);
    //print_data(rp.buffer->data, rp.buffer->size);
    report_add_pair(&rp, "server", int_ntoa(s->tcp->addr.daddr));
    report_add_pair_int(&rp, "server-port", s->tcp->addr.dest);
    report_add_pair(&rp, "client", int_ntoa(s->tcp->addr.saddr));

    // report_add_pair_int(&rp, "loat_packet", s->lost_packets);
    // report_add_pair_int(&rp, "total_packets", s->packets);
// /*    report_add_packet_int(&rp, "packets", "%d/%d",s->lost_packets, s->packets);*/

    report_add_pair(&rp, "status", close_status(type));
    report_add_pair_int(&rp, "req-bytes", s->tcp->server.count);
    report_add_pair_int(&rp, "rep-bytes", s->tcp->client.count);
    report_add_pair_float(&rp, "net-req-time", s->req_time/1000000.0);
    report_add_pair_float(&rp, "server-time", s->server_time/1000000.0);
    report_add_pair_float(&rp, "net-rep-time", s->rep_time/1000000.0);
    if(s->referer){
        report_add_pair(&rp, "referer", s->referer);
        char * referer_host = strstr(s->referer, "//");
        if(referer_host){
            referer_host+=2;
            char *p = strstr(referer_host, "/");
            if(p) *p = 0;
            report_add_pair(&rp, "referer-host", referer_host);
        }
    }
    struct kv *p = s->proplist;
    while(p){
        urldecode(p->value);
        report_add_pair(&rp, kv_type_string(p->type, p->key), p->value);
        p = p->next;
    }

    if(s->is_catch_response_body && s->response_body_size<65535){
        report_add_pair_int(&rp, "body-captured-size", s->response_body_size);
        report_add_key(&rp, "body-captured");

        msgpack_pack_raw(rp.pk, s->response_body_size);
        struct body_buf *b = s->response_body_first;
        int size_pushed = 0;
        while(b){
            msgpack_pack_raw_body(rp.pk, b->data, b->size);
            size_pushed += b->size;
            b = b->next;
        }
    }

    //print_data(rp.buffer->data, rp.buffer->size);
    char *cp = rp.buffer->data;
    cp = cp + 1;
    *cp = rp.count >> 8;
    cp = cp + 1;
    *cp = rp.count % 256;

    //printf("rp.buffer->size=%d\n", rp.count);

    rp.routing_key = malloc(ROUTING_KEY_BUF_SIZE);
    bzero(rp.routing_key, ROUTING_KEY_BUF_SIZE);
    snprintf(rp.routing_key, ROUTING_KEY_BUF_SIZE,
        "http-scope.%s.%dxx.%d", s->host, s->status_code / 100 ,s->status_code);

    send_report(&rp);

    //print_data(rp.buffer->data, rp.buffer->size);
    free(rp.routing_key);

    msgpack_sbuffer_free(rp.buffer);
    msgpack_packer_free(rp.pk);
    session_log(s);
    session_clean(s);
    session_start(s);
}

void session_close(struct session * s, char nids_state){
    session_report(s, nids_state);
    free_item(&s->client_parser);
    free_item(&s->server_parser);
    free(s);
}

void session_time_mark(struct session * s, long * r){
/*    dline();*/
    if(r==NULL){
        gettimeofday(&s->status_time, NULL);
    }else{
        struct timeval tv;
        gettimeofday(&tv, NULL);
        *r += ((tv.tv_sec - s->status_time.tv_sec) * 1000000) + (tv.tv_usec - s->status_time.tv_usec);
        debug("time diff, %d.%06d - %d.%06d  = %d", tv.tv_sec, tv.tv_usec, s->status_time.tv_sec, s->status_time.tv_usec, r);
        memcpy(&s->status_time, &tv, sizeof(struct timeval));
    }
}

void session_handle_data(struct session * s, struct tcp_stream *tcp){
    size_t nparsed;

    if (tcp->client.count_new) {
        //print_data(tcp->client.data, tcp->client.count_new);
        nparsed = http_parser_execute(s->server_parser, &server_parser_settings,
                tcp->client.data, tcp->client.count_new);
        if (s->server_parser->upgrade) {
          debug("/* server handle new protocol */");
        } else if (nparsed != tcp->client.count_new) {
          debug("/* response Handle error. %d != %d */"
                  , nparsed, tcp->client.count_new );
        }
    }else{
        print_data(tcp->server.data, tcp->server.count_new);
        nparsed = http_parser_execute(s->client_parser, &client_parser_settings,
                tcp->server.data, tcp->server.count_new);
        if (s->client_parser->upgrade) {
          debug("/* handle new protocol */");
        } else if (nparsed != tcp->server.count_new) {
          debug("/* request Handle error. %d != %d */"
                  , nparsed, tcp->server.count_new );
        }
    }
}
