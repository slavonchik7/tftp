
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include "tftpbuf.h"

#define BSIZE 10


/* сервер для подключения */
#define SERVOUT_PORT 69
#define SERVOUT_IP "192.168.0.104"

/* ip и порт на котором будет запущен код программы */
#define USER_PORT 2500
#define USER_IP "192.168.0.100"

#define dprint(d, buff, bsize, format, ...) \
            if (__VA_ARGS__  + 0 == NULL) { printf("not\n");} \
            snprintf(buff, bsize, format, __VA_ARGS__ + 0); \
            printf("%d %s", d, buff); \
            return 2;

int main() {

#if 0
    int f = open("testwrite", O_CREAT | O_RDWR, S_IREAD | S_IWRITE);

    char buf[BUFF_SIZE] = "hello world";

    write(f, buf, strlen(buf));

    return 0;
//    char *b = "hello";
//
//    char *a = "hjello";
//
//    a = (char *)mem_rewrite(a, b, 5);
//
//    printf("%s\n", a);
//
//    return 0;


    char buff[BSIZE];
    bzero(buff, BSIZE);

    short a = 1;
    short b, c;

    *(short *)buff = htons(a);
    b = *(short *)buff;

    c = ((short *)buff)[0];

    if (c == b)
        printf("OK\n");

    write(fileno(stdout), buff, BSIZE);
    printf("\n");

    write(fileno(stdout), buff, BSIZE);

    return 0;

#endif // 0








//    struct net_info nfo;

//    tftp_client_init(&nfo, "127.0.0.1:69");




//    struct sockaddr_in sa_user,
//                        sa_serv;
//    int user_fd;
//
//    user_fd = socket(AF_INET, SOCK_DGRAM, 0);
//    if (user_fd == -1)
//        return -13;
//
////    bzero(&sa_user, sizeof(sa_user));
////    sa_user.sin_family = AF_INET;
////    sa_user.sin_port = htons(USER_PORT);
////    inet_pton(AF_INET, "127.0.0.1", &sa_user.sin_addr);
////
////    bind(user_fd, (struct sockaddr *)&sa_user, sizeof(sa_user));
//
//
//    bzero(&sa_serv, sizeof(sa_serv));
//    sa_serv.sin_family = AF_INET;
//    sa_serv.sin_port = htons(SERVOUT_PORT);
//    inet_pton(AF_INET, "127.0.0.1", &sa_serv.sin_addr);
//

//    socklen_t serv_size = sizeof(sa_serv);

//    ssize_t byte_size;
//    size_t mess_size = 1024;
//    char recv_mess[mess_size + 1];
//    recv_mess[mess_size] = 0;
//    bzero(recv_mess, mess_size);
//
//    char *send_mess = NULL;
//    ssize_t bsize = form_rrq_alloc(&send_mess, "fc", "netascii"); //rw_packet_form(&send_mess, 1, "testfile", "netascii");
//    if (bsize == -1)
//        {perror("error form rrq packet\n");
//        }
//        printf("%ld\n", bsize);




    size_t bufsize = 128;
    char mbuf[bufsize];

//    put_file(nfo.s_fd, &nfo.s_addr, "ttest", "wrtest", mbuf, bufsize);
//    get_file(nfo.s_fd, &nfo.s_addr, "fctest", "d", mbuf, bufsize);

//    printf("%s\n", mbuf);

    struct tftp_serv_info tserv_info;

    int errnum;
    if ((errnum = tftp_serv_init(&tserv_info, "127.0.0.1:69", "/home/christmas/Desktop/tftpserver/")) < 0) {

        printf("error start server: %d\nnot such file directory, do you can be forgot \"/\" in the end?\n", errnum);
        return -1;
    } else {
        printf("server init OK\n");
    }

//    sprintf(send_mess, "%hutest\0octet\0", 0x01);
//    sprintf(send_mess, "RRQ");
//    send_mess[3] = 0;

//    byte_size = sendto(user_fd, send_mess, bsize, 0, (struct sockaddr *)&sa_serv, serv_size);
//    if (byte_size == -1) {
//        perror("error send\n");
//    }
//    write(fileno(stdout), send_mess, bsize);
////    printf("send: %s\n", send_mess);
//    byte_size = recvfrom(user_fd, recv_mess, mess_size, 0, (struct sockaddr *)&sa_user, &serv_size);
//    if (byte_size == -1) {
//        perror("error recv\n");
//    }
//    printf("msize: %ld\n strlen: %ld\n", byte_size, strlen(recv_mess));
////    printf("recv get: %s\n", send_mess);
//write(fileno(stdout), recv_mess, mess_size);

    tftp_serv_run(&tserv_info, 3);

    return 0;
}
