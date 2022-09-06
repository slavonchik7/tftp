
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

//#define dprint(d, buff, bsize, format, ...) \
//            if (__VA_ARGS__  + 0 == NULL) { printf("not\n");} \
//            snprintf(buff, bsize, format, __VA_ARGS__ + 0); \
//            printf("%d %s", d, buff); \
//            return 2;

int main() {


    struct tftp_serv_info tserv_info;

    int errnum;
    if ((errnum = tftp_serv_init(&tserv_info, "127.0.0.1:69", "/home/christmas/Desktop/tftpserver/")) < 0) {

        printf("error start server: %d\nnot such file directory, do you can be forgot \"/\" in the end?\n", errnum);
        return -1;
    } else {
        printf("server init OK\n");
    }


    tftp_serv_run(&tserv_info, 10);

    printf("the server is successfully attached\n");

    return 0;
}
