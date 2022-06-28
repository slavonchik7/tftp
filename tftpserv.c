

#include "tftpbuf.h"



int __form_error(void *pbuf, size_t bsize, short err_code, const char *msg);



void *__client_proc(void *cp_data) {
    if (cp_data == NULL)
        return NULL;


    printf("client with pid: %ld started\n", pthread_self());

    struct sc_exch_info *sce_info = (struct sc_exch_info *)cp_data;

    pthread_mutex_lock(&sce_info->__mute_main);
    sce_info->client_process = CLIENT_PROC_BUSY;
    pthread_mutex_unlock(&sce_info->__mute_main);

//    pthread_mutex_lock(&sce_info->__mute_proc);
    sce_info->prc_addr->addr_status = ADDR_ACTIVE;
//    pthread_mutex_unlock(&sce_info->__mute_proc);

    struct saddr_proc *prc_saddr = sce_info->prc_addr;

    short op_code = sce_info->prc_addr->current_op_code = ntohs(((short *)sce_info->buff)[0]);

    /* если обработчик вызван впервые */
    if (sce_info->prc_addr->not_first_call == 0) {
        /* сохраняем в sc_exch_info::saddr_proc в поле current_op_code текущий запрос */
        printf("checked packet client with pid: %ld \n", pthread_self());
        printf("op_code: %hu client with pid: %ld \n", op_code, pthread_self());

        /* в случае, если вместо запроса на чтение или запись пришло что-либо другое */
        if (sce_info->prc_addr->current_op_code != OP_WRQ
                && sce_info->prc_addr->current_op_code != OP_RRQ)
            return NULL;
        printf("check packet succesfull %ld started\n", pthread_self());
        /* говорю, что начинается работа с хостом, с адресом saddr_proc::saddr*/
        sce_info->prc_addr->addr_status = ADDR_ACTIVE;
    }

    int f_fd;
    size_t fpath_len = strlen(sce_info->tftp_dir_path) + strlen(sce_info->buff + 2);
    char fpath[fpath_len + 1];
    snprintf(fpath, fpath_len + 1, "%s%s", sce_info->tftp_dir_path, sce_info->buff + 2);


    /* открытие файла в зависимости от требуемого запроса */
    if (op_code == OP_WRQ) {
        printf("today write\n");
        f_fd = open(fpath, O_CREAT | O_TRUNC | O_WRONLY, S_IRWXG | S_IRWXU | S_IRWXO);
    } else if (op_code == OP_RRQ) {
        f_fd = open(fpath, O_RDONLY);
    } else {
        return NULL;
    }
    printf("open file: %s client with pid: %ld\n", fpath, pthread_self());

    if (f_fd < 0) {
            printf("open file ERROR client with pid: %ld\n", pthread_self());

        __form_error(sce_info->buff, BUFF_SIZE, 0, "Can not open the file for a reading");
        sendto(sce_info->main_fd, sce_info->buff, BUFF_SIZE, 0, (struct sockaddr *)&(sce_info->prc_addr->saddr), sizeof(struct sockaddr_in));
        return NULL;
    }


    printf("open file successfull client with pid: %ld\n", pthread_self());


    ssize_t byte_size;
    short pcount = 0;

    char buf[BUFF_SIZE] = "hello world";

    size_t bwrite = 0;
    size_t bget = 0;



    printf("run proccessing client with pid: %ld\n", pthread_self());
    while (1) {
        printf("--------------------\n");
//        printf("ALL OK client with pid: %ld\n", pthread_self());

        if (sce_info->prc_addr->not_first_call != 0) {

            pthread_mutex_lock(&sce_info->__mute_proc);
            if (sce_info->prc_addr->go_proc == UNPROC) {

                printf("MUTE client with pid: %ld\n", pthread_self());
                pthread_cond_wait(&sce_info->__cond_proc, &sce_info->__mute_proc);
                printf("UNMUTE client with pid: %ld\n", pthread_self());

            }
            pthread_mutex_unlock(&sce_info->__mute_proc);

//            pthread_mutex_lock(&sce_info->__mute_between_proc);
            if (sce_info->prc_addr->ptid != pthread_self()) {
                printf("CONTINUE client with pid: %ld\n", pthread_self());
//                pthread_mutex_unlock(&sce_info->__mute_between_proc);
                continue;
            }
//            pthread_mutex_unlock(&sce_info->__mute_between_proc);

//            pthread_mutex_unlock(&sce_info->__mute_proc);

            pthread_mutex_lock(&sce_info->__mute_main);
            sce_info->client_process = CLIENT_PROC_BUSY;
            pthread_mutex_unlock(&sce_info->__mute_main);
        }



        /* проверка заголовка принятого пакета */
        if (sce_info->prc_addr->current_op_code == OP_ERROR) {
            printf("GETTED ERROR packet client with pid: %ld\n", pthread_self());
            break;
        }


        /* если в данный момент выполняется чтение файла */
        if (op_code == OP_RRQ) {

            /* чтение следующего блока данных */
            if ((byte_size = read(f_fd, sce_info->buff + 4, BUFF_SIZE - 4)) < 0) {
                __form_error(sce_info->buff, BUFF_SIZE, 0, "Can not read data from the file");
                sendto(sce_info->main_fd, sce_info->buff, BUFF_SIZE, 0, (struct sockaddr *)&sce_info->prc_addr->saddr, sizeof(struct sockaddr_in));
                close_return(f_fd, NULL);
            } else if (byte_size == 0) {
                break;
            } else {
                pcount++;
                ((short *)sce_info->buff)[0] = htons(OP_DATA);
                ((short *)sce_info->buff)[1] = htons(pcount);
                printf("send packet data: \"%s\" client with pid: %ld\n", sce_info->buff + 4, pthread_self());
                /* отправка пакета с данными */
                if (sendto(sce_info->main_fd, sce_info->buff, byte_size + 4, 0, (struct sockaddr *)&sce_info->prc_addr->saddr, sizeof(struct sockaddr_in)) < 0) {
                    printf("send packet ERROR client with pid: %ld\n", pthread_self());
                    break;
                }
            }


        /* если в данный момент выполняется запись файла */
        } else if (op_code == OP_WRQ) {
            #if 1
            if (sce_info->prc_addr->not_first_call == 1) {
                /* записываем полученные данные */
                if ((byte_size = write(f_fd, sce_info->buff + 4, sce_info->bsize - 4)) < 0) {
//                if ((byte_size = fwrite(sce_info->buff + 4, 1, sce_info->bsize - BOFFSET, fp)) < 0) {
                    __form_error(sce_info->buff, BUFF_SIZE, 0, "Can not write data to the file");
                    sendto(sce_info->main_fd, sce_info->buff, BUFF_SIZE, 0, (struct sockaddr *)&sce_info->prc_addr->saddr, sizeof(struct sockaddr_in));
                    break;
                } else if (byte_size == 0) {
                    printf(" byte_size == 0");
//                    break;
                } else {

//                    size_t wbyte = 0;
//                    printf("------------------\n");
//                    if (((short *)sce_info->buff)[0] == htons(OP_DATA)) {
//                        printf("03|");
//                    }
////                    if (((short *)sce_info->buff)[1] == htons(1)) {
////                        printf("01");
////                    }
//                    printf("DATA***");
//                    fflush(stdout);
//                    wbyte = write(fileno(stdout), sce_info->buff + 4, sce_info->bsize - 4);
//                    printf("***DATA");fflush(stdout);
//                    printf("recv packet: %ld, wbite: %ld, strlen: %ld", sce_info->bsize, byte_size, strlen(sce_info->buff + 4));
//
//                    printf("\n------------------\n");

                    bwrite += byte_size;

//                    printf("writed packet byte: %ld \"%s\" client with pid: %ld\n", byte_size, pthread_self());
                }
            } else {
                printf("\n---------PACKET--------\n");
                if (((short *)sce_info->buff)[0] == htons(OP_WRQ)) {
                    printf("WRQ");
                }
                write(fileno(stdout), sce_info->buff, sce_info->bsize);
                printf("\n---------PACKET--------\n");
            }
            #endif // 0

            printf("have been writed byte: %ld, packets getted %hu", byte_size, ntohs(((short *)sce_info->buff)[1]));
            printf("form packet ACK client with pid: %ld\n", pthread_self());
            /* формируем ACK пакет и отправляем ответ на принятые даннные */
            ((short *)sce_info->buff)[0] = htons(OP_ACK);
            if (sce_info->prc_addr->not_first_call == 0)
                ((short *)sce_info->buff)[1] = htons(0);

            if (sendto(sce_info->main_fd, sce_info->buff, 4, 0, (struct sockaddr *)&sce_info->prc_addr->saddr, sizeof(struct sockaddr_in)) < 0) {
                printf("send packet ERROR client with pid: %ld\n", pthread_self());
                break;
            }


        }

        bget += sce_info->bsize;





        if (sce_info->prc_addr->not_first_call == 1)
            if (byte_size < BUFF_SIZE - 4) {
//                pthread_mutex_lock(&sce_info->__mute_proc);
                sce_info->prc_addr->addr_status = ADDR_INACTIVE;
//                pthread_mutex_unlock(&sce_info->__mute_proc);
//                break;
            }

        pthread_mutex_lock(&sce_info->__mute_main);
        sce_info->client_process = CLIENT_PROC_FREE;
        pthread_mutex_unlock(&sce_info->__mute_main);

        sce_info->prc_addr->go_proc = UNPROC;

        pthread_cond_broadcast(&sce_info->__cond_main);

        printf("call server for resume\n");

        if (sce_info->prc_addr->not_first_call == 1)
        if (byte_size < BUFF_SIZE - 4) {
            break;
        }

        sce_info->prc_addr->not_first_call = 1;
    }



    printf("ALL DATA GETTED OK client with pid: %ld\n", pthread_self());
    printf("---has been getted byte: %ld, packets %hu, writed: %ld---\n", bget, ntohs(((short *)sce_info->buff)[1]), bwrite);
    close(f_fd);

    printf("WAIT__\n");

    //pthread_mutex_lock(&sc_info.__mute_active_count);
    sce_info->active_host_counter--;
    //pthread_mutex_unlock(&sc_info.__mute_active_count);

    sce_info->prc_addr->not_first_call = 0;
    printf("WAITING IS FINISHED\n");

    return NULL;
}


int __form_error(void *pbuf, size_t bsize, short err_code, const char *msg) {
    if (pbuf == NULL || msg == NULL)
        return -1;


    char *cptr = (char *)pbuf;

    ((short *)cptr)[0] = htons(OP_ERROR);
    ((short *)cptr)[1] = htons(err_code);

    memcpy(cptr + 4, msg, strlen(msg));
//    snprintf(cptr + 4, strlen(msg), msg);
    return 0;
}
