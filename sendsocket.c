#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

int main(int argc , char *argv[])
{

        typedef struct {
                int command;
                //int index;
        } sock;


        struct hostent *h;

        sock out_sock;
        /* check command line args */
        if(argc<3) {
                printf("usage : %s <server> <data1> ... <dataN> \n", argv[0]);
                exit(1);
        }

        /* get server IP address (no check if input is IP address or DNS name */
        h = gethostbyname(argv[1]);
        if(h==NULL) {
                printf("%s: unknown host '%s' \n", argv[0], argv[1]);
                exit(1);
        }

        //socket create
        int sockfd = 0;
        //sockfd = socket(AF_INET , SOCK_DGRAM, 0);
        sockfd = socket(AF_INET , SOCK_STREAM , 0);

        if (sockfd == -1){
                printf("Fail to create a socket.");
        }

        //socket connect

        struct sockaddr_in info;
        bzero(&info,sizeof(info));
        info.sin_family = PF_INET;

        //localhost test
        info.sin_addr.s_addr = inet_addr(argv[1]);
        info.sin_port = htons(atoi(argv[2]));


        int err = connect(sockfd,(struct sockaddr *)&info,sizeof(info));
        if(err==-1){
                printf("Connection error");
        }


        //Send a message to server
        //char message[] = {"stop"};
        //char receiveMessage[100] = {};
        out_sock.command=atoi(argv[3]);
        //out_sock.index=atoi(argv[4]);
        send(sockfd, (void *) &out_sock, sizeof(out_sock), 0);
        //send(sockfd, (void *) &out_sock, sizeof(out_sock), MSG_DONTWAIT);
        close(sockfd);
        return 0;
}
