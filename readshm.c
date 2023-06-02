/*
display shared memory for burstt system
compile with gcc -O3 -o readshm readshm.c -lm -lrt -lhugetlbfs -lnuma -lpthread
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

        // shared memory stuff
        char c;
        int shmid;
        key_t key;
        char* shm, *s;
        struct shm_command *snd_command;
        void *shared_memory = (void *)0;
        struct shmseg *shmp;
	int esc;

        if ((shmid = shmget(SHM_ADD, sizeof(struct shmseg), IPC_CREAT | 0666)) < 0) {
                perror("shmget");
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
	// change to pointer of the structure, and use +1, +2, +3 instead of _0, _1, _2, _3
	// printf("\033[%d;%dH%c", x, y, c);
	printf("\e[?25l"); // to hide the cursor
    	char hostname[1024];
    	gethostname(hostname, 1024);
	time_t tmi;
    	struct tm* utcTime;
        double load_avg[3];

	while(keepRunning){
		time(&tmi);
		utcTime = gmtime(&tmi);
                FILE* fp = fopen("/proc/loadavg", "r");
                if (fp == NULL) {
                        perror("Error opening file");
                        return -1;
                }
		fscanf(fp, "%lf %lf %lf", &load_avg[0], &load_avg[1], &load_avg[2]);
		printf("\033[%d;%dH Server     %s", 0, 0, hostname);
		printf("\033[%d;%dH Time       %4d-%02d-%02dT%02d:%02d:%02d", 2, 0, utcTime->tm_year+1900, utcTime->tm_mon+1, utcTime->tm_mday, (utcTime->tm_hour) % 24, utcTime->tm_min, utcTime->tm_sec );
		printf("\033[%d;%dH Memory     %4lluG / %4lluG", 3, 0, ((get_avphys_pages() * sysconf(_SC_PAGESIZE))/1024/1000000), ((get_phys_pages() * sysconf(_SC_PAGESIZE))/1024/1000000));
		printf("\033[%d;%dH Load avg   %5.2f %5.2f %5.2f", 4, 0, load_avg[0], load_avg[1], load_avg[2]);
		printf("\033[%d;%dH NIC        [0]-> %-15s [1]-> %-15s [2]-> %-15s [3]-> %-15s", 5, 0, shmp->eth_0, shmp->eth_1, shmp->eth_2, shmp->eth_3);
		printf("\033[%d;%dH Status     [0]-> %-15d [1]-> %-15d [2]-> %-15d [3]-> %-15d", 6, 0, *(&shmp->command_0), *(&shmp->command_0+1), *(&shmp->command_0+2), *(&shmp->command_0+3));
		printf("\033[%d;%dH Page       [0]-> %-15d [1]-> %-15d [2]-> %-15d [3]-> %-15d", 7, 0, *(&shmp->page_0), *(&shmp->page_0+1), *(&shmp->page_0+2), *(&shmp->page_0+3));
		printf("\033[%d;%dH CPU        [0]-> %-15d [1]-> %-15d [2]-> %-15d [3]-> %-15d", 8, 0, shmp->cpu_0, *(&shmp->cpu_0+1), *(&shmp->cpu_0+2), *(&shmp->cpu_0+3));
		printf("\033[%d;%dH Block      [0]-> %-15d [1]-> %-15d [2]-> %-15d [3]-> %-15d", 9, 0, shmp->block_0, *(&shmp->block_0+1), *(&shmp->block_0+2), *(&shmp->block_0+3));
		printf("\033[%d;%dH Data rate  [0]-> %-.3f           [1]-> %-.3f           [2]-> %-.3f           [3]-> %-.3f", 10, 0, shmp->dr_0, *(&shmp->dr_0+1), *(&shmp->dr_0+2), *(&shmp->dr_0+3));
		printf("\033[%d;%dH Error code [0]-> %-15d [1]-> %-15d [2]-> %-15d [3]-> %-15d", 11, 0, *(&shmp->err_0), *(&shmp->err_0+1), *(&shmp->err_0+2), *(&shmp->err_0+3));
		usleep(2000);
		fclose(fp);
	}
                if (shmdt(shmp) == -1) {
                        syslog(LOG_INFO, "shmdt failed");
                        exit(EXIT_FAILURE);
                }
                syslog(LOG_INFO, "wrbd exit successfully");
}

