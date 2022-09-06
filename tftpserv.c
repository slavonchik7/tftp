

#include "tftpbuf.h"



int __form_error(void *pbuf, size_t bsize, short err_code, const char *msg);

int __check_open(struct saddr_proc *chf_saddr);


void *__client_proc(void *cp_data) {
    if (cp_data == NULL)
        return NULL;

                    #ifdef DEBUG
    printf("client with pid: %ld started inition\n", pthread_self());
                    #endif // DEBUG

    struct saddr_proc *psaddr_prc = (struct saddr_proc *)cp_data;



    short pcount = 0;

        #ifdef DEBUG
    size_t bwrite = 0;
    size_t bget = 0;
        #endif // DEBUG

    int f_fd;


    struct sc_exch_info *sce_info = psaddr_prc->sc_main;


    int serv_fd = sce_info->srv_info->tftp_sinfo.s_fd;


    size_t salen = sizeof(struct sockaddr_in);

    short mop_code;

    pthread_mutex_t *ptr_mute_proc = &psaddr_prc->__mute_proc;
    pthread_cond_t *ptr_cond_proc = &psaddr_prc->__cond_proc;

    struct sockaddr_in *psock_addr = &psaddr_prc->saddr;


    pthread_mutex_lock(ptr_mute_proc);
    psaddr_prc->ready_status = READY;
    pthread_mutex_unlock(ptr_mute_proc);
    pthread_cond_broadcast(ptr_cond_proc);

                    #ifdef DEBUG
    printf("run proccessing client with pid: %ld\n", pthread_self());
                    #endif // DEBUG



    while (1) {

                            #ifdef DEBUG
        printf("--------------------\n");
                    #endif // DEBUG




        pthread_mutex_lock(ptr_mute_proc);

        if (psaddr_prc->go_proc == UNPROC) {
            #ifdef DEBUG
            printf("MUTE client with pid: %ld\n", pthread_self());
            #endif // DEBUG
            pthread_cond_wait(ptr_cond_proc, ptr_mute_proc);
            #ifdef DEBUG
            printf("UNMUTE client with pid: %ld\n", pthread_self());
            #endif // DEBUG


        }
        pthread_mutex_unlock(ptr_mute_proc);


        if(sce_info->exit_flag) {
            if (!psaddr_prc->first_call) {
                __form_error(sce_info->buff, BUFF_SIZE, 0, "the server was stopped");
                sendto(serv_fd, sce_info->buff, BUFF_SIZE, 0, (struct sockaddr *)psock_addr, salen);
                close(f_fd);
            }
            return NULL;
        }


        char *mbuff = sce_info->buff;




        #ifdef DEBUG
        printf("ALL OK client with pid: %ld\n", pthread_self());
        #endif // DEBUG

        short cop_code = ntohs(((short *)mbuff)[0]);;
        psaddr_prc->current_op_code = cop_code;


        /* проверка заголовка принятого пакета */
        if (cop_code == OP_ERROR) {
            #ifdef DEBUG
            printf("GETTED ERROR packet client with pid: %ld\n", pthread_self());
            #endif // DEBUG
            break;
        }

        if (psaddr_prc->first_call) {

            mop_code = cop_code;

            if ((f_fd = __check_open(psaddr_prc)) < 0)  {
                #ifdef DEBUG
                printf("open file ERROR client with pid: %ld\n", pthread_self());
                #endif // DEBUG

                __form_error(mbuff, BUFF_SIZE, 0, "Can not open the file for a reading");
                sendto(serv_fd, mbuff, BUFF_SIZE, 0, (struct sockaddr *)psock_addr, salen);
                sce_info->active_host_counter--;
                psaddr_prc->first_call = 1;


                psaddr_prc->go_proc = UNPROC;

                pthread_mutex_lock(ptr_mute_proc);
                sce_info->client_process = CLIENT_PROC_FREE;
                pthread_mutex_unlock(ptr_mute_proc);


                pthread_cond_broadcast(ptr_cond_proc);
            }

            #ifdef DEBUG
            printf("open file successfull client with pid: %ld\n", pthread_self());
            fflush(stdout);
            #endif // DEBUG
        }



        ssize_t byte_size;


        /* если в данный момент выполняется чтение файла */
        if (mop_code == OP_RRQ) {

            /* чтение следующего блока данных */
            if ((byte_size = read(f_fd, mbuff + 4, BUFF_SIZE - 4)) < 0) {
                __form_error(mbuff, BUFF_SIZE, 0, "Can not read data from the file");
                sendto(serv_fd, mbuff, BUFF_SIZE, 0, (struct sockaddr *)psock_addr, salen);
                close_return(f_fd, NULL);
            } else {
                pcount++;
                ((short *)mbuff)[0] = N_OP_DATA;
                ((short *)mbuff)[1] = htons(pcount);
                #ifdef DEBUG
                printf("send packet data: \"%s\" client with pid: %ld\n", mbuff + 4, pthread_self());
                #endif // DEBUG
                /* отправка пакета с данными */
                if (sendto(serv_fd, mbuff, byte_size + 4, 0, (struct sockaddr *)psock_addr, salen) < 0) {
                    #ifdef DEBUG
                    printf("send packet ERROR client with pid: %ld\n", pthread_self());
                    #endif // DEBUG
                    break;
                }
            }

        /* если в данный момент выполняется запись файла */
        } else if (mop_code == OP_WRQ) {
            /* записываем полученные данные */
            if (!(psaddr_prc->first_call)) {
                if ((byte_size = write(f_fd, mbuff + 4, sce_info->bsize - 4)) < 0) {
                    __form_error(mbuff, BUFF_SIZE, 0, "Can not write data to the file");
                    sendto(serv_fd, mbuff, BUFF_SIZE, 0, (struct sockaddr *)psock_addr, salen);
                    break;
                } else {
                    #ifdef DEBUG
                    bwrite += byte_size;
                    printf("form packet ACK client with pid: %ld\n", pthread_self());
                    printf("have been writed byte: %ld, packets getted %hu\n", byte_size, ntohs(((short *)mbuff)[1]));
                    #endif // DEBUG
                }
            }
            else {
                ((short *)mbuff)[1] = htons(0);
            }

            /* формируем ACK пакет и отправляем ответ на принятые даннные */
            ((short *)mbuff)[0] = N_OP_ACK;

            if (sendto(serv_fd, mbuff, 4, 0, (struct sockaddr *)psock_addr, salen) < 0) {
                #ifdef DEBUG
                printf("send packet ERROR client with pid: %ld\n", pthread_self());
                #endif // DEBUG
                break;
            }
        }

        #ifdef DEBUG
        bget += sce_info->bsize;
        #endif // DEBUG

        if (!(psaddr_prc->first_call)) {
            if (byte_size < (BUFF_SIZE - 4)) {
//                pthread_mutex_lock(ptr_mute_proc);

                psaddr_prc->first_call = 1;
                psaddr_prc->go_proc = UNPROC;
                close(f_fd);
                #ifdef DEBUG
                printf("CLOSE file client with pid: %ld\n", pthread_self());
                #endif // DEBUG
                sce_info->active_host_counter--;

//                pthread_mutex_unlock(ptr_mute_proc);
            }
        } else {
            #ifdef DEBUG
            printf("I want go first with pid: %ld\n", pthread_self());
            #endif // DEBUG
            psaddr_prc->first_call = 0;
        }

        #ifdef DEBUG
        printf("call server for resume  with pid: %ld\n", pthread_self());
        #endif // DEBUG


        psaddr_prc->go_proc = UNPROC;

        pthread_mutex_lock(ptr_mute_proc);
        sce_info->client_process = CLIENT_PROC_FREE;
        pthread_mutex_unlock(ptr_mute_proc);


        pthread_cond_broadcast(ptr_cond_proc);
    }

    return NULL;
}


int __form_error(void *pbuf, size_t bsize, short err_code, const char *msg) {
    if (pbuf == NULL || msg == NULL)
        return -1;

    char *cptr = (char *)pbuf;

    ((short *)cptr)[0] = htons(N_OP_ERROR);
    ((short *)cptr)[1] = htons(err_code);

    memcpy(cptr + 4, msg, strlen(msg));

    return 0;
}



int __check_open(struct saddr_proc *chf_saddr) {
    if (chf_saddr == NULL)
        return -1;

//    struct sc_exch_info *sce_info = chf_saddr->sc_main;
    char *fpbuff = chf_saddr->sc_main->buff;

    char *pdir = chf_saddr->sc_main->srv_info->tftp_dir;

    int f_fd = -1;
    size_t fpath_len = strlen(pdir) + strlen(fpbuff + 2);
    char fpath[fpath_len + 1];

    snprintf(fpath, fpath_len + 1, "%s%s", pdir, fpbuff + 2);


    short op_code = chf_saddr->current_op_code;
                            #ifdef DEBUG
    printf("---------FILE NAME: %s + %s ------\n", sce_info->tftp_dir_path, fpbuff + 2);
                    #endif // DEBUG
    /* открытие файла в зависимости от требуемого запроса */
    if (op_code == OP_WRQ) {
                                #ifdef DEBUG
        printf("today write\n");
                    #endif // DEBUG
        f_fd = open(fpath, O_CREAT | O_TRUNC | O_WRONLY /*, S_IRWXG | S_IRWXU | S_IRWXO */ );
    } else if (op_code == OP_RRQ) {
                                    #ifdef DEBUG
        printf("today read\n");
                    #endif // DEBUG
        f_fd = open(fpath, O_RDONLY);
    } else {
                                #ifdef DEBUG
        printf("Getted full shit op_code: %hu \n", op_code);
                    #endif // DEBUG
    }


    return f_fd;
}




