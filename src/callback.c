//#define debug
//#define debug_line
#include <string.h>
#include <stdlib.h>

#include "defined.h"
#include "session.h"

#define IS_MULTIPART 2

char * parse_string(char split, const char *p, size_t l, char type
        , char * tester, unsigned short tester_len, struct session * s
        , char catch_end_of_length){
/*    print_trace(); */
    print_data(p, l);

    #define in_ready 0
    #define in_key 1
    #define in_value_ignore 2
    #define in_value 3

    int i=0, j=0, kl=0, k=in_ready;
    char *ks = 0, *c;


    for(i=0;i<l;i++){
        switch(k){
            case in_ready:
                if(*(p+i)>'A'){
                    k = in_key;
                    j = i;
                    ks = p + i;
                }
                break;

            case in_key:
                if(*(p+i)=='='){
                    if(is_in_list(p+j, i-j, tester, tester_len)){
                        k = in_value;
                        kl = i-j;
                    }else{
                        k = in_value_ignore;
                    }
                    j = i+1;
                }else if(*(p+i)==split){
                    k = in_ready;
                }
                break;

            case in_value:
            case in_value_ignore:
                //debug("char=%c, i=%d, j=%d", *(p+i), i, j);
                if((i==(l-1) && catch_end_of_length && i++) || *(p+i)==split || *(p+i)==0){
                    if(k==in_value){
                        struct kv *item = malloc(sizeof(struct kv));
                        item->key = strndup(ks, kl);
                        strtolower(item->key);
                        item->type = type;
                        item->value = strndup(p+j, i-j);
                        item->next = s->proplist;
                        s->proplist = item;
                    }
                    if(i<l && *(p+i)==0){
                        return 0;
                    }
                    k = in_ready;
                }
        }

        if(i<l && *(p+i)==0){
            return 0;
        }else if(i==(l-1)){
            return ks;
        }
    }

    return 0;
}

int request_url(http_parser* p, const char *at, size_t length){
    struct session * s = p->data;
    s->path = strndup(at, length);
    if(config.catch_get_len > 0){
        char * p = strchr(s->path, '?');
        if(p){
            parse_string('&', p+1, length, T_GET, config.catch_get, config.catch_get_len, s, 1);
        }
    }
    return 0;
}

#define FIELD_HOST 1
#define FIELD_REFERER 2
#define FIELD_UA 3
#define FIELD_COOKIE 4
#define FIELD_CONTENT_TYPE 5

int request_header_field(http_parser* p, const char *at, size_t length){
    struct session * s = p->data;
    if(is_in_list(at, length, config.catch_req_head, config.catch_req_head_len)){
        s->last_request_header = strndup(at, length);
        strtolower(s->last_request_header);
    }

    if(config.catch_cookie_len>0 && s->catched_cookie==0 && (strncasecmp(at, "cookie", length)==0)){
        s->cur_field = FIELD_COOKIE;
    }

    if(config.catch_post_len>0 && s->request_multipart==0 && (strncasecmp(at, "content-type", length)==0)){
        s->cur_field = FIELD_CONTENT_TYPE;
    }

    if(strncasecmp("host", at, length)==0){
        s->cur_field = FIELD_HOST;
    }else if(strncasecmp("referer", at, length)==0){
        s->cur_field = FIELD_REFERER;
    }else if(strncasecmp("user-agent", at, length)==0){
        s->cur_field = FIELD_UA;
    }

    return 0;
}

int request_headers_complete(http_parser* p){
    struct session * s = p->data;
    s->method = p->method;
    s->http_major = p->http_major;
    s->http_minor = p->http_minor;
    /* requests only */
    return 0;
}

int request_header_value(http_parser* p, const char *at, size_t length){
    struct session * s = p->data;
    if(s->last_request_header){

        struct kv *h = malloc(sizeof(struct kv));
        h->key = s->last_request_header;
        h->value = strndup(at, length);
        h->next = s->proplist;
        h->type = T_REQUEST_HEADER;
        s->proplist = h;
        s->last_request_header = 0;
    }
    
    switch(s->cur_field){
    case FIELD_COOKIE:
        parse_string(';', at, length, T_COOKIE, config.catch_cookie, config.catch_cookie_len, s, 1);
        break;

    case FIELD_CONTENT_TYPE:
        if(strncasecmp(at, "multipart", length)>=0){
            s->request_multipart = IS_MULTIPART;
        }else{
            s->request_multipart = 3;
        }
        break;
    
    case FIELD_HOST:
        s->host = strndup(at, length);
        break;

    case FIELD_REFERER:
        s->referer = strndup(at, length);
        break;

    case FIELD_UA:
        s->ua = strndup(at, length);
    }
    s->cur_field = 0;
    return 0;
}

#define min(a, b) (a>b?b:a)
int request_body(http_parser* parser, char *at, size_t length){
    struct session * s = parser->data;

    if(config.catch_post_len>0){
        if(s->request_multipart == IS_MULTIPART){
            debug("multipart");
        }else{
            char *p = 0;

            if(s->post_buf_len>0){
                int last_post_buf_len = s->post_buf_len;
                buf_push(s, at, length);
                p = parse_string('&', s->post_buf, s->post_buf_len, T_POST, config.catch_post, config.catch_post_len, s, 0);
                if(p > s->post_buf){
                    int next = p - s->post_buf - last_post_buf_len;
                    at += next;
                    length -= next;
                    s->post_buf_len = 0;
                }else if(p==0){
                    s->post_buf_len = 0;
                                }
            }

	if(s->post_buf_len == 0){
		p = parse_string('&', at, length, T_POST, config.catch_post, config.catch_post_len, s, 0);
		if(p){
			int more = length-(p-at);
			if(more<POST_BUF_LENGTH){
				buf_push(s, p, more);
			}
		}
	}
        }
    }
    return 0;
}

buf_push(struct session * s, char *p, int len){
    len = min(len, POST_BUF_LENGTH-s->post_buf_len-1);
    if(!s->post_buf){
        s->post_buf = malloc(POST_BUF_LENGTH);
    }
    memcpy(s->post_buf + s->post_buf_len, p, len);
    s->post_buf_len += len;
    //s->post_buf[s->post_buf_len] = 0;
    //*p = s->post_buf;
}

/*-------------------------------------------------------------------------*/

int reponse_header_field(http_parser* p, const char *at, size_t length){
    struct session * s = p->data;
    if(is_in_list(at, length, config.catch_rsp_head, config.catch_rsp_head_len)){
        s->last_response_header = strndup(at, length);
        strtolower(s->last_response_header);
    }
    return 0;
}

int response_headers_complete(http_parser* p){
    struct session * s = p->data;
    s->status_code = p->status_code;
    s->is_catch_response_body = config.catch_code[p->status_code];
    //printf("1:%X, %d, %s\n",s, s->status_code, s->host);
    return 0;
}

int reponse_header_value(http_parser* p, const char *at, size_t length){
    struct session * s = p->data;
    if(s->last_response_header){
        struct kv *h = malloc(sizeof(struct kv));
        h->key = s->last_response_header;
        h->value = strndup(at, length);
        h->next = s->proplist;
        h->type = T_RESPONSE_HEADER;
        s->proplist = h;
        s->last_response_header = 0;
    }
    return 0;
}

int request_begin(http_parser* p){
    struct session * s = p->data;
    s->need_report = 1;
    session_time_mark(s, NULL);

    time(&(s->start_time));
    unsigned short i = s->start_time % RPS_N;
    if(stat->rps[i].time == s->start_time){
        stat->rps[i].value++;
    }else{
        stat->rps[i].time = s->start_time;
        stat->rps[i].value = 1;
    }
    return 0;
}

int request_complete(http_parser* parser){
    struct session * s = parser->data;
    session_time_mark(s, &s->req_time);
    if(config.catch_post_len>0 && s->post_buf_len>0){
        parse_string('&', s->post_buf, s->post_buf_len, T_POST, config.catch_post, config.catch_post_len, s, 1);
    }
    return 0;
}

int response_begin(http_parser* p){
    struct session * s = p->data;
    session_time_mark(s, &s->server_time);
    return 0;
}

int response_complete(http_parser* p){
    if(p->status_code /100 != 1){
        struct session * s = p->data;
        session_time_mark(s, &s->rep_time);
        session_report(s, 0);
    }
    return 0;
}

int response_body(http_parser* p, const char *at, size_t length){
    struct session * s = p->data;
    if(s->is_catch_response_body){
        if(s->response_body_size + length < 65535){
            struct body_buf *b= malloc(sizeof(struct body_buf));

            b->next = 0;
            b->data = malloc(length);
            memcpy(b->data, at, length);
            b->size = length;

            if(s->response_body_first){
                s->response_body_last->next = b;
            }else{
                s->response_body_first = b;
            }
            s->response_body_last = b;
            s->response_body_size += length;
        }
    }
    return 0;
}

/*-------------------------------------------------------------------------*/

void session_global_init(){
    client_parser_settings.on_message_begin = request_begin;
    client_parser_settings.on_message_complete = request_complete;
    client_parser_settings.on_headers_complete = request_headers_complete;
    client_parser_settings.on_url = request_url;
    client_parser_settings.on_header_field = request_header_field;
    client_parser_settings.on_header_value = request_header_value;
    client_parser_settings.on_body = request_body;

    server_parser_settings.on_message_begin = response_begin;
    server_parser_settings.on_message_complete = response_complete;
    server_parser_settings.on_headers_complete = response_headers_complete;
    server_parser_settings.on_header_field = reponse_header_field;
    server_parser_settings.on_header_value = reponse_header_value;
    server_parser_settings.on_body = response_body;
}
