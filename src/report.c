//#define debug
//#define debug_line
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include "defined.h"
#include "session.h"

int pipe_fd[2];

void report_pthread(){
	short size;
	while(read(pipe_fd[0], &size, 2)){

	}
}

int connect_master(){
    int status;
    printf("Connecting master %s:%d... \n", config.master.host, config.master.port);
    return -1;
    config.master.conn = amqp_new_connection();
    config.master.socket = amqp_tcp_socket_new(config.master.conn);
    if (!config.master.socket) {
        die("creating TCP socket");
    }
    status = amqp_socket_open(config.master.socket, config.master.host, config.master.port);
    if (status) {
        return -1;
    }

    die_on_error(amqp_login(config.master.conn, config.master.vhost, 0, 131072, 0, 
            AMQP_SASL_METHOD_PLAIN, config.master.user, config.master.password),
                    "AMQPErr: Logging in");
    amqp_channel_open(config.master.conn, 1);
    die_on_error(amqp_get_rpc_reply(config.master.conn), "AMQPErr: Opening channel");

    amqp_exchange_declare(config.master.conn, 1, 
        amqp_cstring_bytes(config.master.exchange), amqp_cstring_bytes("topic"),
                        0, 1, amqp_empty_table);
    die_on_error(amqp_get_rpc_reply(config.master.conn), "AMQPErr: Declaring exchange");
    config.master.connected = 1;
    return config.master.connected;
}

static pthread_mutex_t swaplock;
void send_report(struct msg_report* rp){
    // pthread_mutex_lock(&swaplock);
    // if(amqp_send_report(rp) < 0){
    // 	if(config.swap_fd){
	   //      fwrite(rp->buffer->data, rp->buffer->size, 1, config.swap_fd);
    // 	}
    // }
    // pthread_mutex_unlock(&swaplock);
}

int start_report_pthread(){
	int tid;
	pipe(pipe_fd);
	printf("PIPE_BUF=%d\n", PIPE_BUF);
    connect_master();
    return pthread_create(&tid, NULL, report_pthread, NULL);
}

int amqp_send_report(struct msg_report* rp){
    if(config.master.connected){
        amqp_basic_properties_t props;
        props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG;
        props.content_type = amqp_cstring_bytes("application/msgpack");
        props.delivery_mode = 2; /* persistent delivery mode */

        amqp_bytes_t messagebody;
        messagebody.len = rp->buffer->size;
        messagebody.bytes = rp->buffer->data;

        return amqp_basic_publish(config.master.conn,
                                        1,
                                        amqp_cstring_bytes(config.master.exchange),
                                        amqp_cstring_bytes(rp->routing_key),
                                        0,
                                        0,
                                        &props,
                                        messagebody);
    }else{
        return -1;
    }
}
