#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdlib.h>
#include "defined.h"

#define debug_line 1
#define SHMEM_SIZE  8*1024*1024

int statfd;
int shmfd;
char is_master;
const char buff[100];

const char *shm_id(){
    char *p = config.pid_file;
    unsigned int hash=0;
    for(p;*p;p++){
        hash = hash*33 + *p;
    }
    sprintf(&buff, "bscope.%d", hash % 65535);
    return &buff;
}

int status_init(char create){
    is_master = create;
    if(!config.pid_file){
        printf("Where's pid location?\n");
        exit(-1);
    }
    if(create){
        statfd = createPidFile("bscope", config.pid_file, 1);
        shm_unlink(shm_id());
    }
    shmfd = shm_open(shm_id(), create?(O_RDWR | O_CREAT):O_RDONLY, 0644);
    if (shmfd == -1){
        if(create){
            exit(-1);
        }else{
            printf("fail to get shmfd. shm_id():%s\n", shm_id());
            return -1;
        }
    }
    if (create && (ftruncate(shmfd, SHMEM_SIZE) == -1)){ perror("shared memory"); exit(-1); }
    stat = (status *)mmap(0, SHMEM_SIZE,create?(PROT_READ | PROT_WRITE):PROT_READ, MAP_SHARED, shmfd, 0);
    if (create){
        bzero(stat, SHMEM_SIZE);
        stat->pid = getpid();
        stat->alive = 1;
    }
    return 1;
}

int status_shutdown(){
    if(is_master){
        stat->alive = 0;
        unlink(config.pid_file);
    }
    close(statfd);
    close(shmfd);
    shm_unlink(shm_id());
}

int status_write(){
    return 0;
}

int status_mon() {
    if (status_init(0) == -1) {
      printf("fail to get shmfd. shm_id():%s\n", shm_id());
      return -1;
    }
    time_t ctime;
    time(&ctime);

    ctime % RPS_N;
    unsigned int i, n, t2,minite_n=0, all_n=0, minite_v=0, all_v=0;
    for(i=1;i<RPS_N;i++){
        t2 = ctime - i;
        n = t2 % RPS_N;
        if(stat->rps[n].time == t2){
            if(i<61){
		minite_v+=stat->rps[n].value;
		minite_n++;
	    }
            all_v += stat->rps[n].value;
	    all_n ++;
        }
    }
    printf("Connection: %d\n", stat->conn);
    printf("RPS: %d,  %.2f,  %.2f\n",
            stat->rps[(ctime-1) % RPS_N].value,
            (0.0+minite_v) / 60,
            (0.0+all_v) / RPS_N
          );
    return 0;
}

int push_data(char * p, char * data, unsigned short size){
    unsigned short *n = p;
    *n = size;
    p+=2;
    memcpy(p, data, size);
    stat->p += size + 2;
}

int status_logpush(char * msg, unsigned short size){
    unsigned int offset = (void *)&stat->data  - (void *)stat;
    if( (size + stat->p + 4 ) >= (SHMEM_SIZE - offset) ){
        unsigned short *n = &stat->data + stat->p;
        *n = 65535;
        stat->p = 0;
        stat->life = (stat->life+1) % 255;
        push_data(&stat->data, msg, size+1);
    }else{
        push_data(&stat->data + stat->p, msg, size+1);
    }
}

int show_log(char i, unsigned int p, char need_flush){
    unsigned short l;
    if(i==stat->life && p==stat->p){
        if(need_flush) fflush(stdout);
        usleep(1000*1000);
        if(stat->alive){
            return show_log(i, p, 0);
        }else{
            status_shutdown();
            return status_logcat();
        }
    }else{
        unsigned short *size = &stat->data + p;
        l = *size;
        if(l > LOG_BUF_SIZE || (i!=stat->life && stat->p > p)){
            show_log(stat->life, 0, 0);
        }else{
            char *c = &stat->data + p + 2;
            printf("%s\n", c);
            return show_log(i, p + 2 + l, 1);
        }
    }
}

int status_logcat(){
    if(status_init(0)>0){
        char *p = &stat->data + stat->p;
        return show_log(stat->life, 0, 0);
    }else{
        sleep(1);
        return status_logcat();
    }
}
