//#define debug
#define debug_line
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#include "defined.h"
#include "session.h"

#define CFG_HOST_NAME 0
#define CFG_HOST_PORT 1
#define CFG_HOST_IP 2
#define CFG_CATCH_CODE 3
#define CFG_CATCH_REQ_HEAD 4
#define CFG_CATCH_RSP_HEAD 5
#define CFG_CATCH_COOKIE 6
#define CFG_CATCH_GET 7
#define CFG_CATCH_POST 8

#define CONST_STR_LEN(s) s, sizeof(s) - 1

void handler_hup(){
    printf("reloading config\n");
    loadconfig();
}

void prepare_catch_code(char*p){
    int i, j, k;
    #define test(n) ((n>='0' && n<='9') || n=='x')
    if(test(p[0]) && test(p[1]) && test(p[2])){
        for(i=0;i<10;i++){
            for(j=0;j<10;j++){
                for(k=0;k<10;k++){
                    if((i==(p[0]-'0') || p[0]=='x') && (j==(p[1]-'0') || p[1]=='x') && (k==(p[2]-'0') || p[2]=='x')){
                        config.catch_code[i*100 + j*10 + k] = 1;
                    }
                }
            }
        }
    }
}

int ip_parse(int j, char *str){
    unsigned int i=0, ip=0, mask_bits=0;
    char *strbak = strdup(str);
    char *p = strchr(str, '.');
    for(i=3;i>0;i--){
        if(NULL == p){
            fprintf(stderr, "bad ip address: %s\n", strbak); 
            exit(1);
        }else{
             *p = '\0';
        }
        ip += (atoi(str) << (8*i));
        str = p+1;
        p = strchr(str, '.');
    }
    p = strchr(str, '/');
    config.hostip[j][1] = 0xffffffff;

    if(p){
        *p = '\0';
        config.hostip[j][1]  = (atoi(p+1)==0)?0:config.hostip[j][1] << (32 - atoi(p+1));
    }
    ip += atoi(str);
    config.hostip[j][0] = ip & config.hostip[j][1];
    free(strbak);
    return ip;
}

void split_list(char *value, int type){
    const char * split = ", ";
    char *p = strtok (value, split);
    while(p) {
        if(type == CFG_HOST_NAME){
            config.hostname[config.hostname_len++] = p;
        }else if(type==CFG_HOST_PORT){
            config.hostport[config.hostport_len++] = atoi(p);
        }else if(type == CFG_CATCH_REQ_HEAD){
            config.catch_req_head[config.catch_req_head_len++] = p;
        }else if(type == CFG_CATCH_RSP_HEAD){
            config.catch_rsp_head[config.catch_rsp_head_len++] = p;
        }else if(type == CFG_CATCH_COOKIE){
            config.catch_cookie[config.catch_cookie_len++] = p;
        }else if(type == CFG_CATCH_GET){
            config.catch_get[config.catch_get_len++] = p;
        }else if(type == CFG_CATCH_POST){
            config.catch_post[config.catch_post_len++] = p;
        }else if(type==CFG_HOST_IP){
            ip_parse(config.hostip_len++, p);
        }else if(type==CFG_CATCH_CODE){
            prepare_catch_code(p);
        }
        p = strtok(NULL,split);
    }
}

static int handler_config(void* file, const char* section, const char* name, const char* value){
    configuration* pconfig = (configuration*)file;

    #define MATCH(s, n) strcasecmp(section, s) == 0 && strcasecmp(name, n) == 0
    if (MATCH("master", "host")) {
        pconfig->master.host = strdup(value);
    } else if (MATCH("master", "user")) {
        pconfig->master.user = strdup(value);
    } else if (MATCH("master", "password")) {
        pconfig->master.password = strdup(value);
    } else if (MATCH("master", "vhost")) {
        pconfig->master.vhost = strdup(value);
    } else if (MATCH("master", "exchange")) {
        pconfig->master.exchange = strdup(value);
    } else if (MATCH("master", "port")) {
        pconfig->master.port = atoi(value);
    } else if (MATCH("master", "swap_file")) {
        pconfig->swap_fd = fopen(value, "a+");
    } else if (MATCH("http-scope", "node-name")) {
        pconfig->node = strdup(value);
    } else if (MATCH("http-scope", "host-name")) {
        split_list(strdup(value), CFG_HOST_NAME);
    } else if (MATCH("http-scope", "catch-content-by-code")) {
        split_list(strdup(value), CFG_CATCH_CODE);
    } else if (MATCH("http-scope", "catch-request-header")) {
        split_list(strdup(value), CFG_CATCH_REQ_HEAD);
    } else if (MATCH("http-scope", "catch-response-header")) {
        split_list(strdup(value), CFG_CATCH_RSP_HEAD);
    } else if (MATCH("http-scope", "catch-cookie")) {
        split_list(strdup(value), CFG_CATCH_COOKIE);
    } else if (MATCH("http-scope", "catch-get")) {
        split_list(strdup(value), CFG_CATCH_GET);
    } else if (MATCH("http-scope", "catch-post")) {
        split_list(strdup(value), CFG_CATCH_POST);
    } else if (MATCH("http-scope", "host-ip")) {
        split_list(strdup(value), CFG_HOST_IP);
    } else if (MATCH("http-scope", "host-port")) {
        split_list(strdup(value), CFG_HOST_PORT);
    } else if (MATCH("http-scope", "daemon-mode")) {
        if(pconfig->daemon_mode==0){
            pconfig->daemon_mode = (strcmp("yes", value)==0)?1:-1;
        }
    } else if (MATCH("http-scope", "packet-checksum")) {
        nids_params.skip_checksum = (strcmp("yes", value)==0)?0:1;
    } else if (MATCH("http-scope", "pid-file")) {
        if(pconfig->pid_file==0){
            pconfig->pid_file = strdup(value);
        }
    } else if (MATCH("http-scope", "network_interface")) {
        if(pconfig->netif==0){
            pconfig->netif = strdup(value);
        }
    } else {
        return 0;  /* unknown section/name, error */
    }
    return 1;
}

int loadconfig(){
    int file=0;
    if((file=open(config.config_file, O_RDONLY)) < 0){
        perror(config.config_file);
        exit(-1);
    }
 
    config.node = "unnamed";
    config.master.host = "localhost";
    config.master.user = "guest";
    config.master.password = "guest";
    config.master.vhost = "/";
    config.master.exchange = "http-scope";
    config.master.port = 5672;

    return ini_parse(config.config_file, handler_config, &config);
}

int test_in_service(struct tuple4 * addr){
    int i, j;
    for(i=0;i<config.hostip_len;i++){
        if((ntohl(addr->daddr) & config.hostip[i][1]) == config.hostip[i][0]){
            for(j=0;j<config.hostport_len;j++){
                if(config.hostport[j] == addr->dest){
                    return 1;    
                }    
            }
        }
    }
    return 0;
}

void frag_callback(struct ip * a_packet, int len){ ; }

void tcp_callback (struct tcp_stream *tcp, void ** this_time_not_needed) {

    pid_t ppid;
    if(!test_in_service(&tcp->addr)) return;

    switch(tcp->nids_state){

        case NIDS_JUST_EST:
            stat->conn++;

            ppid = getppid();
            if(ppid<5){
                exit(1);
            }

            ;struct session * s = malloc(sizeof(struct session));

            s->server_parser = malloc(sizeof(http_parser));
            s->client_parser = malloc(sizeof(http_parser));
            s->tcp = tcp;
            session_start(s);

            tcp->user = s;

            tcp->client.collect++;
            tcp->server.collect++;
            break;

        case NIDS_DATA:
            session_handle_data(tcp->user, tcp);
            break;

        case NIDS_CLOSE:
        case NIDS_RESET:
        case NIDS_TIMED_OUT:
            stat->conn--;
            session_close(tcp->user, tcp->nids_state);
            break;

    }
    return ;
}

static void diep(char *s){
    perror(s);
    exit(1);
}
    
static void show_version () {
	write(1, CONST_STR_LEN(
		"Build-Date: " __DATE__ " " __TIME__ "\n"
	));
}

static void show_help () {
	write(1, CONST_STR_LEN(
		"Usage: http-scope [-c config] [-fd]\n" \
		"\n" \
		"Options:\n" \
		" -c <config_file>  " CONF "\n" \
		" -p <pid_file> \n" \
		" -i <network_interface>    eth0/eth1....\n" \
		" -f console mode\n" \
		" -d daemon mode\n" \
		" -0 disable supervisor process\n" \
		" -v show version\n" \
		" -h show help\n" \
        " \n" \
		"Tools:\n" \
		" -l show logs\n" \
		" -s show status\n" \
	));
}

void empty(){
}

int start_nids(){
    config.master.connected = 0;
    session_global_init();

    nids_params.device = config.netif;
	nids_params.syslog = empty;
    nids_params.multiproc = 1;
    printf("working at %s\n", nids_params.device);
    if (!nids_init ()) {
        fprintf(stderr,"%sn",nids_errbuf);
        exit(1);
    }

    nids_register_ip_frag(frag_callback);
    nids_register_tcp(tcp_callback);
    nids_run ();
}

pid_t child_pid;

void handler_exit(){
    if(child_pid){
        kill(child_pid, SIGTERM);
    }else{
        status_shutdown();
        exit(1);
    }
}

int supervisor_loop(){
    pid_t pid = fork();
    if(pid==0){
        child_pid = 0;
        return start_nids();
    }else if(pid>0){
        int status;
        child_pid = pid;
        waitpid(pid, &status, 0);
        if(WIFSIGNALED(status)){
            printf("child %d terminate(%d), restarting..\n", pid, WTERMSIG(status));
            supervisor_loop();
        }
    }else{
        exit(1);
    }
}

int start_worker(){
    start_report_pthread();
	if(config.supervisor_mode){
		return supervisor_loop();
	}else{
		return start_nids();
	}
}

int main(int argc, char* argv[]){
    int o, mode=0;

    #define LOG_MODE 1
    #define STAT_MODE 2

    bzero(&config, sizeof(configuration));
    config.supervisor_mode = 1;
    config.config_file = CONF;

	while (-1 != (o = getopt(argc, argv, "c:p:i:df?vh0ls"))) {
		switch(o) {
    		case 'c': config.config_file = optarg; break;
    		case 'p': config.pid_file = optarg; break;
    		case 'f': config.daemon_mode = -1; break;
    		case 'd': config.daemon_mode = 1; break;
    		case '0': config.supervisor_mode = 0; break;
    		case 'v': show_version(); return 0;
    		case 'i': config.netif = optarg; break;


    		case 'l': mode = LOG_MODE; break;
    		case 's': mode = STAT_MODE; break;

    		case '?':
    		case 'h': show_help(); return 0;
    		default: show_help(); return -1;
    	}
	}
    loadconfig();
    uid_t uid;

    switch(mode){
        case LOG_MODE:
            return status_logcat();
        case STAT_MODE:
            return status_mon();
        default:
            uid = getuid();
            if(uid!=0){
                write(1, CONST_STR_LEN("Error, need root\n"));
                exit(-1);
            }
 
            signal(SIGINT, handler_exit);
            signal(SIGTERM, handler_exit);
            status_init(1);

            if(config.daemon_mode==1){
                pid_t pid = fork();
                if(pid==0){
                    return start_worker();
                }else if(pid>0){
                    exit(0);
                }else{
                    exit(1);
                }
            }else{
                return start_worker();
            }
    }
}

/* vim: set ts=4 sw=4 tw=0: */
