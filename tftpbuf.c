
#include "tftpbuf.h"



static volatile int sig_exit_flag = 0;

static int serv_break(struct saddr_proc *saddrs, ssize_t ssize);

extern int __form_error(void *pbuf, size_t bsize, short err_code, const char *msg);

struct saddr_proc *__find_saddr(struct saddr_proc *sproc, ssize_t ssize, struct sockaddr_in *taddr);
struct saddr_proc *__find_free(struct saddr_proc *sproc, ssize_t ssize);
int __addr_is_active(struct saddr_proc *sproc, size_t ssize, struct sockaddr_in *taddr);


void exit_sighandler(int signum, siginfo_t *si, void *v);

void *__client_proc(void *cp_data);

void *pwait_create(struct saddr_proc *wait_saddr);

size_t form_rrq(void *pbuf, size_t bsize, const char *file_name, const char *trans_mode) {
    return rw_packet_form(pbuf, bsize, OP_RRQ, file_name, trans_mode);
}

size_t form_wrq(void *pbuf, size_t bsize, const char *file_name, const char *trans_mode) {
    return rw_packet_form(pbuf, bsize, OP_WRQ, file_name, trans_mode);
}

size_t rw_packet_form(void *pbuf, size_t bsize, const short rw_code, const char *file_name, const char *trans_mode) {
    char *buf_ptr = pbuf;

    size_t fn_size = strlen(file_name) + 1;
    size_t mode_size = strlen(trans_mode) + 1;

    bzero(buf_ptr, bsize);

    ((short *)buf_ptr)[0] = htons(rw_code);
    buf_ptr += 2;

    memcpy(buf_ptr, file_name, fn_size);
    buf_ptr += fn_size;
    memcpy(buf_ptr, trans_mode, mode_size);

    return 2 + fn_size + mode_size;
}

int get_file(int sockfd, struct sockaddr_in *ptr_saddr, const char *load_fpath, const char *upload_fpath, char *messg, size_t msize) {
    if (load_fpath == NULL || upload_fpath == NULL)
        return -1;

    struct sockaddr_in serv_saddr;

    int loadfd = open(load_fpath, O_CREAT | O_WRONLY, S_IREAD | S_IWRITE);

    if (loadfd < 0) {
        snprintf(messg, msize, "Can not open file %s", load_fpath);
        return -1;
    }

    char buff[BUFF_SIZE];
    char *wbuff = buff + 4;
    bzero(buff, BUFF_SIZE);

    ssize_t byte_size;
    ssize_t write_size;


    socklen_t sock_len = sizeof(struct sockaddr_in);

    if (sendto(sockfd, buff, form_rrq(buff, BUFF_SIZE, upload_fpath, MODE_ASC), 0,
                                (struct sockaddr *)ptr_saddr, sock_len) < 0) {
        msg_cls_rtrn(loadfd, -1, messg, msize,
                "Error sendto(), function return < 0 about the time sending RRQ packet%s", "");
    }

    size_t getted_byte_size = 0;

    do {

        byte_size = recvfrom(sockfd, buff, BUFF_SIZE, 0, (struct sockaddr*)&serv_saddr, &sock_len);

        /* во время приема данных, закачки пакета, мы получаем либо пакет DATA, либо ERROR
         * соответственно проверяем на эти пакеты */
        if (byte_size < 0) {
            msg_cls_rtrn(loadfd, -1, messg, msize, "Error recvfrom(), function returned < 0%s", "");
        }
        if (ntohs(((short *)buff)[0]) == OP_ERROR) {
            msg_cls_rtrn(loadfd, -1, messg, msize, buff + 4);
        }

        /* запись в файл
         * проверем, чтобы в файл было записано столько же, сколько получили */
         write_size = write(loadfd, wbuff, byte_size - 4);
        if (write_size != byte_size - 4) {
            if (write_size < 0)
                snprintf(messg, msize, "Error write(), function returned < 0, about the time of writing to the file%s", "");
            else
                snprintf(messg, msize, "Error sendto(), function write less than recv%s", "");
            close_return(loadfd, -1)
        }

        getted_byte_size += byte_size - 4;

        /* формирование ACK пакета */
        ((short *)buff)[0] = htons(OP_ACK);

        /* отправка ACK пакета */
        if (sendto(sockfd, buff, 4, 0, (struct sockaddr *)&serv_saddr, sock_len) < 0) {
            msg_cls_rtrn(loadfd, -1, messg, msize, "Error sendto(), function return < 0%s", "");
        }

    } while(byte_size == BUFF_SIZE);

    msg_cls_rtrn(loadfd, 0, messg, msize, "getted and writed packet/byte: %hu/%ld", ntohs(((short *)buff)[1]), getted_byte_size);
}

int put_file(int sockfd, struct sockaddr_in *ptr_saddr, const char *upload_fpath, const char *load_fpath, char *messg, size_t msize) {
    if (load_fpath == NULL || upload_fpath == NULL)
        return -1;

    static struct sockaddr_in serv_saddr;

    int uploadfd = open(upload_fpath, O_RDONLY);

    if (uploadfd < 0) {
        snprintf(messg, msize, "Can not open file %s", upload_fpath);
        return -1;
    }

    char buff[BUFF_SIZE];
    char *rbuff = buff + 4;
    bzero(buff, BUFF_SIZE);

    ssize_t byte_size;

    ssize_t read_size;

    socklen_t sock_len = sizeof(struct sockaddr_in);

    if (sendto(sockfd, buff, form_wrq(buff, BUFF_SIZE, load_fpath, MODE_ASC), 0,
                    (struct sockaddr *)ptr_saddr, sock_len)  < 0) {
        msg_cls_rtrn(uploadfd, -1, messg, msize, "Error sendto(), function return < 0 about the time sending WRQ packet%s", "");
    }

    size_t putted_byte_size = 0;
    short pcount = 0;

     while (1) {


        /* после отправки запроса WRQ или пакета DATA, в ответ получаем ACK или ERROR
         * проверка полученного пакета */
        if (recvfrom(sockfd, buff, BUFF_SIZE, 0, (struct sockaddr*)&serv_saddr, &sock_len) < 0) { // || ntohs(((short *)buff)[0]) == OP_ERROR || ntohs(((short *)buff)[0]) != OP_ACK) {
            msg_cls_rtrn(uploadfd, -1, messg, msize, "Error recvfrom(), function returned < 0%s", "");
        }
        if (ntohs(((short *)buff)[0]) == OP_ERROR) {
            msg_cls_rtrn(uploadfd, -1, messg, msize, "%s", buff + 4);
        }

        /* чтение следйющего блока данных из файла */
        if ((read_size = read(uploadfd, rbuff, BUFF_SIZE - 4)) < 0) {
            msg_cls_rtrn(uploadfd, -1, messg, msize, "Error read(), function return < 0, about the time of reading from the file%s", "");
        } else if (read_size > 0) {
            pcount++;
        } else {
            break;
        }

        /* формирование пакета данных */
        ((short *)buff)[0] = htons(OP_DATA);
        ((short *)buff)[1] = htons(pcount); /* номер блока */

        /* отправка данных */
        byte_size = sendto(sockfd, buff, read_size + 4, 0, (struct sockaddr *)&serv_saddr, sock_len);
        if (byte_size - 4 != read_size) {
            if (byte_size < 0)
                snprintf(messg, msize, "Error sendto(), function return < 0%s", "");
            else
                snprintf(messg, msize, "Error sendto(), function sent less than read%s", "");

            close_return(uploadfd, -1);
        }

        putted_byte_size += read_size;
    }

    msg_cls_rtrn(uploadfd, 0, messg, msize, "readed and putted packet/byte: %hu/%ld", pcount, putted_byte_size);
}



int udp_addrin_init(struct net_info *serv_info, const char *ip_port_str) {
    if (serv_info == NULL || ip_port_str == NULL)
        return -1;

    char *ptr_port = strstr(ip_port_str, ":");
    if (ptr_port == NULL)
        return -2;

    ptr_port++;
    printf("%s\n", ip_port_str);

    serv_info->s_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (serv_info->s_fd < 0)
        return -3;


    bzero(&serv_info->s_addr, sizeof(struct sockaddr_in));
    serv_info->s_addr.sin_family = AF_INET;
    serv_info->s_addr.sin_port = htons(atoi(ptr_port));
    if (inet_pton(AF_INET, ip_port_str, &serv_info->s_addr.sin_addr) < 0)
        return -4;

    if (bind(serv_info->s_fd, (struct sockaddr *)&serv_info->s_addr, sizeof(struct sockaddr_in)) < 0)
        return -5;

    return 0;
}


int tftp_client_init(struct net_info *serv_info, const char *serv_ip_port_str) {
    if (serv_info == NULL || serv_ip_port_str == NULL)
        return -1;

    return udp_addrin_init(serv_info, serv_ip_port_str);
}

int tftp_serv_init(struct tftp_serv_info *serv_info, const char *ip_port_str, const char *tftp_path) {
    if (serv_info == NULL || ip_port_str == NULL || tftp_path == NULL)
        return -1;

    int errnum;
    /* проверка существования директории */
    if (opendir(tftp_path) == NULL)
        return -2;

    if ( (errnum = udp_addrin_init(&serv_info->tftp_sinfo, ip_port_str)) < 0) {
        fprintf(stderr, "Error init udp_addr_init(), %d\n", errnum);
        return -3;
    }

    serv_info->tftp_dir = NULL;
    if ((serv_info->tftp_dir = mem_rewrite(serv_info->tftp_dir, tftp_path, strlen(tftp_path) + 1)) == NULL) {
        close(serv_info->tftp_sinfo.s_fd);
        return -4;
    }

    return 0;
}

void *mem_rewrite(void *ptr_dest, const void *ptr_src, size_t src_size) {
    if (ptr_src == NULL)
        return NULL;

    if ((ptr_dest = realloc(ptr_dest, src_size)) == NULL)
        return NULL;

    if (memcpy(ptr_dest, ptr_src, src_size) == NULL)
        return NULL;

    return ptr_dest;
}

void print_host(struct sockaddr_in *sinfo) {
    char ipstr[INET6_ADDRSTRLEN];
    void *paddr = &sinfo->sin_addr;
    inet_ntop(sinfo->sin_family, paddr, ipstr, sizeof ipstr);
    printf("host [%s:%d]\n", ipstr, ntohs(sinfo->sin_port));
}


int tftp_serv_run(struct tftp_serv_info *serv_info, const size_t max_cnnct_number) {
    if (serv_info == NULL)
        return -1;
    if (max_cnnct_number == 0)
        return 0;


    struct sc_exch_info sc_info;
    sc_info.exit_flag = 0;
    sc_info.srv_info = serv_info;
    sc_info.client_process = CLIENT_PROC_FREE;
    sc_info.active_host_counter = 0;

    printf("mcount:%zu\n", max_cnnct_number);

    struct saddr_proc saddrs[max_cnnct_number];

    struct saddr_proc *prev_saddr;
    struct saddr_proc *tmp_saddr = saddrs;



    for(size_t i = 0; i < max_cnnct_number; i++) {
        pthread_cond_init(&(saddrs[i].__cond_proc), NULL);
        pthread_mutex_init(&(saddrs[i].__mute_proc), NULL);

        saddrs[i].ready_status = NOT_READY;
        saddrs[i].first_call = 1;
        saddrs[i].go_proc = UNPROC;

        saddrs[i].sc_main = &sc_info;

        pthread_create(&saddrs[i].ptid, NULL, __client_proc, &saddrs[i]);

        pwait_create(&saddrs[i]);
    }


    int serv_fd = serv_info->tftp_sinfo.s_fd;

    fd_set serv_setfd;
    FD_ZERO(&serv_setfd);
    FD_SET(serv_fd, &serv_setfd);



    char buff[BUFF_SIZE];
    sc_info.buff = buff;



    sigset_t sa_mask;

    sigemptyset(&sa_mask);
    sigaddset(&sa_mask, SIGINT);
    pthread_sigmask(SIG_BLOCK, &sa_mask, NULL);


    struct sigaction sa;
    bzero(&sa, sizeof(sa));
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = exit_sighandler;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGINT, &sa, NULL);


    sigset_t empty_mask;
    sigemptyset(&empty_mask);


//    struct sockaddr_in client_addr;
//    socklen_t sck_len = sizeof(struct sockaddr_in);
//    socklen_t *ptr_sck_len = &sck_len;
//
//    struct sockaddr_in *ptr_client_addr = &client_addr;

    while (pselect(serv_fd + 1, &serv_setfd, NULL, NULL, NULL, &empty_mask)) {


        pthread_mutex_lock(&tmp_saddr->__mute_proc);
        if (sc_info.client_process == CLIENT_PROC_BUSY) {
                            #ifdef DEBUG
            printf("serv in MUTE\n");
                    #endif // DEBUG
            /* ожидаю, пока потоки завершат свою работу */
            pthread_cond_wait(&tmp_saddr->__cond_proc, &tmp_saddr->__mute_proc);
                    #ifdef DEBUG
            printf("serv has been UNMUTE\n");
                    #endif // DEBUG
        }
        pthread_mutex_unlock(&tmp_saddr->__mute_proc);


        if(sig_exit_flag) {
            serv_break(saddrs, max_cnnct_number);
            return 1;
        }


        struct sockaddr_in client_addr;
        struct sockaddr_in *ptr_client_addr = &client_addr;

        socklen_t sck_len = sizeof(struct sockaddr_in);
        socklen_t *ptr_sck_len = &sck_len;



//        bzero(buff, BUFF_SIZE);

        prev_saddr = tmp_saddr;
        /* принимаю сообщения от хоста */
        if ((sc_info.bsize = recvfrom(serv_fd, buff, BUFF_SIZE, 0, (struct sockaddr *)ptr_client_addr, ptr_sck_len)) <= 0)
            continue;
                                #ifdef DEBUG
        printf("Geted packet\n");
                    #endif // DEBUG


        /* проверка хоста от которого были получены данные
         * и принятие решения о дальнейших действиях */

                    #ifdef DEBUG
        print_host(&client_addr);
                    #endif // DEBUG

        if ((tmp_saddr = __find_saddr(saddrs, max_cnnct_number, ptr_client_addr)) != NULL) {
                            #ifdef DEBUG
            printf("such host already have been connected\n");
                    #endif // DEBUG

//            pthread_mutex_lock(&tmp_saddr->__mute_proc);
            tmp_saddr->go_proc = START_PROC;
            sc_info.client_process = CLIENT_PROC_BUSY;
                                #ifdef DEBUG
            printf("host have a first call: %d\n", tmp_saddr->first_call);
                    #endif // DEBUG
//            pthread_mutex_unlock(&tmp_saddr->__mute_proc);
            pthread_cond_broadcast(&tmp_saddr->__cond_proc);

                                #ifdef DEBUG
            printf("client has been call resume\n");
                    #endif // DEBUG
        } else if (sc_info.active_host_counter < max_cnnct_number) {
                    #ifdef DEBUG
            printf("current client did not existed\n");
                    #endif // DEBUG
            tmp_saddr = __find_free(saddrs, max_cnnct_number);
                    #ifdef DEBUG
            printf("host have a first call: %d\n", tmp_saddr->first_call);
                    #endif // DEBUG
//            pthread_mutex_lock(&tmp_saddr->__mute_proc);
            sc_info.client_process = CLIENT_PROC_BUSY;

            tmp_saddr->saddr = client_addr;
            tmp_saddr->go_proc = START_PROC;

            sc_info.active_host_counter++;

//            pthread_mutex_unlock(&tmp_saddr->__mute_proc);

            pthread_cond_broadcast(&tmp_saddr->__cond_proc);

        } else {
            tmp_saddr = prev_saddr;
            /* если с такого хоста раннее ничего не приходило и мест больше нет
             * ничего не делаем */
            __form_error(buff, BUFF_SIZE, 0, "Exceeded the maximum number of server clients");
            sendto(serv_fd, buff, BUFF_SIZE, 0, (struct sockaddr *)ptr_client_addr, sck_len);
                                 # ifdef DEBUG
             fprintf(stderr, "---do not have a free client space---");
                    #endif // DEBUG
        }

                    #ifdef DEBUG
        printf("Waiting a packet...\n");
                    #endif // DEBUG

    }

    return 0;
}


struct saddr_proc *__find_saddr(struct saddr_proc *sproc, ssize_t ssize, struct sockaddr_in *taddr) {
    if (sproc == NULL || taddr == NULL)
        return NULL;

    static size_t salen = sizeof(struct sockaddr_in);

    while(--ssize >= 0)
        if (!memcmp(taddr, &((sproc[ssize]).saddr), salen))
            return &(sproc[ssize]);

    return NULL;
}


struct saddr_proc *__find_free(struct saddr_proc *sproc, ssize_t ssize) {
    if (sproc == NULL)
        return NULL;


    while(--ssize >= 0)
        if (sproc[ssize].first_call)
            return &(sproc[ssize]);

    return NULL;
}

int __addr_is_active(struct saddr_proc *sproc, size_t ssize, struct sockaddr_in *taddr) {
    if (sproc == NULL || taddr == NULL)
        return -1;

    for (size_t i = 0; i < ssize; i++) {
        if (memcmp(taddr, &((sproc[i]).saddr), sizeof(struct sockaddr_in)) == 0) {
            if (sproc[i].first_call == 1)
                return 0;
            else
                return -1;
        }
    }

    return -1;
}


int serv_break(struct saddr_proc *saddrs, ssize_t ssize) {
    if (saddrs == NULL)
        return -1;

    struct sc_exch_info *sc_info = saddrs->sc_main;
    sc_info->exit_flag = 1;

    /* completing and closing all client threads */
    while(--ssize >= 0) {
        pthread_cond_broadcast(&(saddrs[ssize].__cond_proc));
        pthread_join(saddrs[ssize].ptid, NULL);

        pthread_mutex_destroy(&(saddrs[ssize].__mute_proc));
        pthread_cond_destroy(&(saddrs[ssize].__cond_proc));
    }

    /* clearing the memory allocated for storing the server pathс*/
    free(sc_info->srv_info->tftp_dir);

    /* closing the main socket of the server */
    close(sc_info->srv_info->tftp_sinfo.s_fd);



    return 0;
}


void *pwait_create(struct saddr_proc *wait_saddr) {
    if (wait_saddr == NULL)
        return NULL;

    pthread_mutex_lock(&wait_saddr->__mute_proc);
    if (wait_saddr->ready_status == NOT_READY) {
        pthread_cond_wait(&wait_saddr->__cond_proc, &wait_saddr->__mute_proc);
    }
    pthread_mutex_unlock(&wait_saddr->__mute_proc);

    return wait_saddr;
}

void exit_sighandler(int signum, siginfo_t *si, void *v) {
    sig_exit_flag = 1;
}






