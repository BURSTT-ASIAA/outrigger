/*
receive command from socket and update to shared memory
compile with gcc -O3 -o sock2shmd sock2shmd.c -lm -lrt -lhugetlbfs -lnuma -lpthread
 */ 
#include "burstt.h"


// Signal handler function
void handleSignal(int signal) {
    if (signal == SIGTERM || signal == SIGINT) {
        // Set the flag to stop the daemon
        keepRunning = false;
    }
}


int
main(int argc, char* argv[])
{
        // change to the "/" directory
        int nochdir = 0;

        // redirect standard input, output and error to /dev/null
        // this is equivalent to "closing the file descriptors"
        int noclose = 0;

        // glibc call to daemonize this process without a double fork
        if(daemon(nochdir, noclose))
                perror("daemon");

        // handle kill -15
        signal(SIGTERM, handleSignal);

        typedef struct {
                int command;
        } sock;

        // socket stuff
        // socket create
        sock in_sock;
        int sockfd = 0,forClientSockfd = 0;
        sockfd = socket(AF_INET , SOCK_STREAM , 0);

        if (sockfd == -1){
                syslog(LOG_ERR, "Fail to create a socket.");
                printf("Fail to create a socket.");
        }

        //socket connection
        struct sockaddr_in serverInfo,clientInfo;
        unsigned int addrlen = sizeof(clientInfo);
        bzero(&serverInfo,sizeof(serverInfo));

        serverInfo.sin_family = AF_INET;
        serverInfo.sin_addr.s_addr = INADDR_ANY;
        serverInfo.sin_port = htons(SOCK_PORT);
        bind(sockfd,(struct sockaddr *)&serverInfo,sizeof(serverInfo));
        listen(sockfd,5);

        // shared memory stuff
        char c;
        int shmid;
        key_t key;
        char* shm, *s;
        struct shm_command *snd_command;
        void *shared_memory = (void *)0;
        struct shmseg *shmp;

        if ((shmid = shmget(SHM_ADD, sizeof(struct shmseg), IPC_CREAT | 0666)) < 0) {
                perror("shmget");
		syslog(LOG_INFO,"shmget failed");
                exit(1);
        }

        if ((shmp = shmat(shmid, NULL, 0)) == (void*) - 1) {
                perror("shmat");
                exit(1);
        }

        syslog(LOG_INFO, "Memory attached at %X", (int *)shmp);
        struct shared_use *shared_stuff;
        shared_stuff = (struct shared_use *)shared_memory;

        int status=0;

	// clear shared memory
        sprintf(shmp->eth_0,"%s", "\0");
        sprintf(shmp->eth_1,"%s", "\0");
        sprintf(shmp->eth_2,"%s", "\0");
        sprintf(shmp->eth_3,"%s", "\0");
	shmp->command_0=0;
	shmp->command_1=0;
	shmp->command_2=0;
	shmp->command_3=0;
	shmp->page_0=0;
	shmp->page_1=0;
	shmp->page_2=0;
	shmp->page_3=0;
        shmp->block_0=0;
        shmp->block_1=0;
        shmp->block_2=0;
        shmp->block_3=0;
        shmp->cpu_0=0;
        shmp->cpu_1=0;
        shmp->cpu_2=0;
        shmp->cpu_3=0;
        shmp->dr_0=0;
        shmp->dr_1=0;
        shmp->dr_2=0;
        shmp->dr_3=0;
        shmp->err_0 = 0;
        shmp->err_1 = 0;
        shmp->err_2 = 0;
        shmp->err_3 = 0;

        // daemon function
        while(keepRunning) {
                forClientSockfd = accept(sockfd,(struct sockaddr*) &clientInfo, &addrlen);
                recv(forClientSockfd,(void *) &in_sock,sizeof(in_sock),0);
                syslog(LOG_INFO, "Receiving command: %d", in_sock.command);
                status = in_sock.command;
		//if( (shmp->command_0!=3) && (shmp->command_0!=3) && (shmp->command_0!=3) && (shmp->command_0!=3)) {
                //	shmp->command_0 = (shmp->command_0 | in_sock.command);
                //	shmp->command_1 = (shmp->command_1 | in_sock.command);
                //	shmp->command_2 = (shmp->command_2 | in_sock.command);
                //	shmp->command_3 = (shmp->command_3 | in_sock.command);
		//}
		shmp->command_0 = in_sock.command;
		shmp->command_1 = in_sock.command;
		shmp->command_2 = in_sock.command;
		shmp->command_3 = in_sock.command;
		close(forClientSockfd);
		usleep(1000);
        }

        if (shmdt(shmp) == -1) {
        	syslog(LOG_INFO, "shmdt failed");
                exit(EXIT_FAILURE);
        }
        syslog(LOG_INFO, "sock2shmd exit successfully");

}

