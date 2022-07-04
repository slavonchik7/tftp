

#ifndef TFTPBUF_H
#define TFTPBUF_H 1


#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <malloc.h>

/* TFTP op-codes */
#define OP_RRQ		1
#define OP_WRQ		2
#define OP_DATA		3
#define OP_ACK		4
#define	OP_ERROR	5

#define MODE_ASC "netascii"
#define MODE_OCT "octet"
#define MODE_MAIL "mail"

#define BUFF_SIZE 516

/* для поля addr_status структуры saddr_working
 * если с адресом saddr ещё происходит общение, то ADDR_ACTIVE
 * если закончилась передача файла или что-либо ещё, то заканчиваю общение ADDR_INACTIVE */
#define ADDR_INACTIVE   11
#define ADDR_ACTIVE     12

#define CLIENT_PROC_FREE 21
#define CLIENT_PROC_BUSY 22

#define START_PROC 31
#define UNPROC 32

#define READY       41
#define NOT_READY   42

#define PROC_WORK   51
#define PROC_END_WORK   52


#define close_return(fd, val) \
            close(fd); \
            return val;

#define msg_cls_rtrn(fd, val, buff, bsize, format, ...) \
            snprintf(buff, bsize, format, __VA_ARGS__ + 0); \
            close_return(fd, val);


struct net_info {
    struct sockaddr_in s_addr;
    int s_fd;
};

struct tftp_serv_info {
    struct net_info tftp_sinfo;
    char *tftp_dir;
};

struct saddr_proc {
    pthread_cond_t  __cond_proc;
    pthread_mutex_t __mute_proc;

    pthread_t ptid;

    pthread_cond_t  __cond_swait;
    pthread_mutex_t __mute_swait;

    volatile int ready_status;

    struct sockaddr_in saddr;

    int addr_status;
    int first_call;
    short current_op_code;

    volatile int go_proc;

    volatile int work_proc;

    struct sc_exch_info *sc_main;
};

struct sc_exch_info {

    int active_host_counter;

    char *tftp_dir_path;

    volatile int client_process;

    int main_fd;

    struct saddr_proc *prc_addr;

    struct sockaddr_in *serv_saddr;

    char *buff;
    volatile ssize_t bsize;
};

void print_host(struct sockaddr_in *sinfo);

size_t form_rrq(void *pbuf, size_t bsize, const char *file_name, const char *trans_mode);
size_t form_wrq(void *pbuf, size_t bsize, const char *file_name, const char *trans_mode);
ssize_t form_ack(void **pbuf);

size_t rw_packet_form(void *pbuf, size_t bsize, const short rw_code, const char *file_name, const char *trans_mode);

int get_file(int sockfd, struct sockaddr_in *ptr_saddr, const char *load_fpath, const char *upload_fpath, char *messg, size_t msize);
int put_file(int sockfd, struct sockaddr_in *ptr_saddr, const char *upload_fpath, const char *load_fpath, char *messg, size_t msize);


/* not using */
ssize_t rw_packet_form_alloc(void **pbuf, const short rw_code, const char *file_name, const char *trans_mode);

void *mem_rewrite(void *ptr_dest, const void *ptr_src, size_t src_size);

/* ip, port format  "ip:port", for example ("192.168.1.1:8080") */
int tftp_client_init(struct net_info *serv_info, const char *ip_port_str);
int udp_addrin_init(struct net_info *serv_info, const char *ip_port_str);
//int udp_client_init();

int tftp_serv_init(struct tftp_serv_info *serv_info, const char *ip_port_str, const char *tftp_path);
int tftp_serv_run(struct tftp_serv_info *serv_info, const size_t max_cnnct_number);

int tftp_serv_wr_to_client(struct tftp_serv_info *serv_info, struct sockaddr_in *client_addrin);
int tftp_serv_rd_from_client(struct tftp_serv_info *serv_info, struct sockaddr_in *client_addrin);


#endif // TFTPBUF_H
