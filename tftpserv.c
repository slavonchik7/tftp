

#include "tftpbuf.h"



int __form_error(void *pbuf, size_t bsize, short err_code, const char *msg);

int __check_open(struct saddr_proc *chf_saddr);


void *__client_proc(void *cp_data) {
    if (cp_data == NULL)
        return NULL;


    printf("client with pid: %ld started inition\n", pthread_self());

    struct saddr_proc *psaddr_prc = (struct saddr_proc *)cp_data;

    #if 0
    pthread_mutex_lock(&psaddr_prc->__mute_proc);
    psaddr_prc->ready_status = NOT_READY;
    pthread_mutex_unlock(&psaddr_prc->__mute_proc);
    #endif // 0




    ssize_t byte_size;
    short pcount = 0;

    size_t bwrite = 0;
    size_t bget = 0;

    int f_fd;


    struct timespec tmspec;


    tmspec.tv_nsec = 0;
    tmspec.tv_sec = 1;


    struct sc_exch_info *sce_info = psaddr_prc->sc_main;


    size_t salen = sizeof(struct sockaddr_in);

    short mop_code;
    short cop_code;

    pthread_mutex_lock(&psaddr_prc->__mute_proc);
    psaddr_prc->ready_status = READY;
    pthread_mutex_unlock(&psaddr_prc->__mute_proc);
    pthread_cond_broadcast(&psaddr_prc->__cond_proc);

    printf("run proccessing client with pid: %ld\n", pthread_self());
    while (1) {
        printf("--------------------\n");
//        printf("ALL OK client with pid: %ld\n", pthread_self());


        pthread_mutex_lock(&psaddr_prc->__mute_swait);
        psaddr_prc->work_proc = PROC_END_WORK;
        sce_info->client_process = CLIENT_PROC_FREE;
        pthread_mutex_unlock(&psaddr_prc->__mute_swait);

        pthread_cond_broadcast(&psaddr_prc->__cond_swait);


        pthread_mutex_lock(&psaddr_prc->__mute_proc);

        if (psaddr_prc->go_proc == UNPROC) {

            printf("MUTE client with pid: %ld\n", pthread_self());
            pthread_cond_wait(&psaddr_prc->__cond_proc, &psaddr_prc->__mute_proc);
            printf("UNMUTE client with pid: %ld\n", pthread_self());

        }
        sce_info->client_process = CLIENT_PROC_BUSY;

        psaddr_prc->work_proc = PROC_WORK;

        pthread_mutex_unlock(&psaddr_prc->__mute_proc);


        char *mbuff = sce_info->buff;
        struct sockaddr_in *psock_addr = &psaddr_prc->saddr;


        printf("ALL OK client with pid: %ld\n", pthread_self());
        psaddr_prc->current_op_code = ntohs(((short *)mbuff)[0]);;
        cop_code = psaddr_prc->current_op_code;


        /* проверка заголовка принятого пакета */
        if (cop_code == OP_ERROR) {
            printf("GETTED ERROR packet client with pid: %ld\n", pthread_self());
            break;
        }

        if (psaddr_prc->first_call) {

            mop_code = cop_code;

            if ((f_fd = __check_open(psaddr_prc)) < 0)  {
                printf("open file ERROR client with pid: %ld\n", pthread_self());

                __form_error(mbuff, BUFF_SIZE, 0, "Can not open the file for a reading");
                sendto(sce_info->main_fd, mbuff, BUFF_SIZE, 0, (struct sockaddr *)&psock_addr, salen);
                return NULL;
            }

            printf("open file successfull client with pid: %ld\n", pthread_self());

        }



        /* если в данный момент выполняется чтение файла */
        if (mop_code == OP_RRQ) {

            /* чтение следующего блока данных */
            if ((byte_size = read(f_fd, mbuff + 4, BUFF_SIZE - 4)) < 0) {
                __form_error(mbuff, BUFF_SIZE, 0, "Can not read data from the file");
                sendto(sce_info->main_fd, mbuff, BUFF_SIZE, 0, (struct sockaddr *)psock_addr, salen);
                close_return(f_fd, NULL);
            } else if (byte_size == 0) {
                break;
            } else {
                pcount++;
                ((short *)mbuff)[0] = htons(OP_DATA);
                ((short *)mbuff)[1] = htons(pcount);
                printf("send packet data: \"%s\" client with pid: %ld\n", mbuff + 4, pthread_self());
                /* отправка пакета с данными */
                if (sendto(sce_info->main_fd, mbuff, byte_size + 4, 0, (struct sockaddr *)psock_addr, salen) < 0) {
                    printf("send packet ERROR client with pid: %ld\n", pthread_self());
                    break;
                }
            }

        /* если в данный момент выполняется запись файла */
        } else if (mop_code == OP_WRQ) {
            #if 1
//            if (sce_info->prc_addr->not_first_call == 1) {
            /* записываем полученные данные */
            if (!psaddr_prc->first_call) {
                if ((byte_size = write(f_fd, mbuff + 4, sce_info->bsize - 4)) < 0) {
                    __form_error(mbuff, BUFF_SIZE, 0, "Can not write data to the file");
                    sendto(sce_info->main_fd, mbuff, BUFF_SIZE, 0, (struct sockaddr *)psock_addr, salen);
                    break;
                } else if (byte_size == 0) {
                    printf(" byte_size == 0");

                } else {

                    bwrite += byte_size;
                }
                printf("have been writed byte: %ld, packets getted %hu\n", byte_size, ntohs(((short *)mbuff)[1]));
                printf("form packet ACK client with pid: %ld\n", pthread_self());
            }
            else {
                ((short *)mbuff)[1] = htons(0);
            }
//            } else {
//                printf("\n---------PACKET--------\n");
//                if (((short *)mbuff)[0] == htons(OP_WRQ)) {
//                    printf("WRQ");
//                }
//                write(fileno(stdout), mbuff, sce_info->bsize);
//                printf("\n---------PACKET--------\n");
//            }
            #endif // 0


            /* формируем ACK пакет и отправляем ответ на принятые даннные */
            ((short *)mbuff)[0] = htons(OP_ACK);

            if (sendto(sce_info->main_fd, mbuff, 4, 0, (struct sockaddr *)psock_addr, salen) < 0) {
                printf("send packet ERROR client with pid: %ld\n", pthread_self());
                break;
            }
        }

        bget += sce_info->bsize;





        if (!psaddr_prc->first_call) {
            if (byte_size < BUFF_SIZE - 4) {
                pthread_mutex_lock(&psaddr_prc->__mute_proc);

                psaddr_prc->addr_status = ADDR_INACTIVE;
                psaddr_prc->first_call = 1;
                close(f_fd);
                sce_info->active_host_counter--;

                pthread_mutex_unlock(&psaddr_prc->__mute_proc);
            }
        } else
            psaddr_prc->first_call = 0;

//        pselect(0, NULL, NULL, NULL, &tmspec, NULL);



        printf("call server for resume\n");


        #if 0
        pthread_mutex_lock(&psaddr_prc->__mute_proc);
        sce_info->client_process = CLIENT_PROC_FREE;
        pthread_mutex_unlock(&psaddr_prc->__mute_proc);
        #endif // 0

        psaddr_prc->go_proc = UNPROC;

        pthread_cond_broadcast(&psaddr_prc->__cond_proc);
    }

    return NULL;
}


int __form_error(void *pbuf, size_t bsize, short err_code, const char *msg) {
    if (pbuf == NULL || msg == NULL)
        return -1;


    char *cptr = (char *)pbuf;

    ((short *)cptr)[0] = htons(OP_ERROR);
    ((short *)cptr)[1] = htons(err_code);

    memcpy(cptr + 4, msg, strlen(msg));

    return 0;
}



int __check_open(struct saddr_proc *chf_saddr) {
    if (chf_saddr == NULL)
        return -1;

    struct sc_exch_info *sce_info = chf_saddr->sc_main;

    int f_fd = -1;
    size_t fpath_len = strlen(sce_info->tftp_dir_path) + strlen(sce_info->buff + 2);
    char fpath[fpath_len + 1];

    snprintf(fpath, fpath_len + 1, "%s%s", sce_info->tftp_dir_path, sce_info->buff + 2);


    short op_code = chf_saddr->current_op_code;
    printf("---------OP_CODE: %hu ------\n", op_code);
    /* открытие файла в зависимости от требуемого запроса */
    if (op_code == OP_WRQ) {
        printf("today write\n");
        f_fd = open(fpath, O_CREAT | O_TRUNC | O_WRONLY, S_IRWXG | S_IRWXU | S_IRWXO);
    } else if (op_code == OP_RRQ) {
        printf("today read\n");
        f_fd = open(fpath, O_RDONLY);
    }


    return f_fd;
}




