/*
rudpd with single normal-sized hugepage, and stop receiving packets while saving ring buffer
compile with gcc -O3 -mavx -o rudpd rudpd.c -lm -lrt -lhugetlbfs -lnuma -lpthread
*/
#include "burstt.h"

unsigned long packet_size = 8256;
unsigned long packet_number = 800000;
unsigned int block = 20;
unsigned long block_size;
unsigned long long z, ivalue;
unsigned int opened=0;

int 	status=0;
bool 	loop=true;

typedef struct {
                int command;
        } r_sock;

struct shmseg *shmp;

void setBit(unsigned char *bufstart, unsigned int index) {
        unsigned int p=0;
        unsigned int bit;
        unsigned char *buf;
        p = index/8;
        bit = index % 8;
        buf = bufstart +  p;
        *buf = *buf | 1 << bit;
}

int set_low_latfency() {
	uint32_t lat = 0;
	int fd = open("/dev/cpu_dma_latency", O_RDWR);
	if (fd == -1) {
		fprintf(stderr, "Failed to open cpu_dma_latency: error %s", strerror(errno));
		return fd;
	}
	write(fd, &lat, sizeof(lat));
	return fd;
}


void sigroutine (int sig) {
	printf("\nGot signal %d\n", sig);
	printf("Stop receiving packets shortly...\n\n");
	syslog(LOG_INFO, "Got signal %d", sig);
	syslog(LOG_INFO, "Stop receiving packets shortly");
	kill (0,SIGTERM);
}

void watchdog()
{
	//loop = false;
	status = -1;
	//shmp->command_0+fpga = -1;
	syslog(LOG_INFO, "Receiving Packets Timeout");
	kill (0,SIGTERM);
}

void low_data_rate()
{
        loop = false;
        status = -1;
	//shmp->command = -1;
        syslog(LOG_INFO, "Process terminated due to low data rate");
	kill (0,SIGTERM);

}

void mq_error()
{       
        loop = false;
        status = -1;
	//shmp->command = -1;
        syslog(LOG_INFO, "Message Queue Error");
	kill (0,SIGTERM);

}

int main(int argc, char *argv[]) 
{

        // change to the "/" directory
        int nochdir = 0;

        // redirect standard input, output and error to /dev/null
        // this is equivalent to "closing the file descriptors"
        int noclose = 0;

        // glibc call to daemonize this process without a double fork
        if(daemon(nochdir, noclose))
                perror("daemon");

	struct shmseg *shmp_timer;
	int shmid, shmid_timer;
	unsigned int port=60000;
        int sockfd;
	//struct timespec start, end;
	clock_t start, end, wdtimer1, wdtimer2;
        unsigned int i, j, first=1;
        struct sockaddr_in server_addr;         // server address information
        struct sockaddr_in fpga_addr;   	// fpga's address information
        int addr_len, numbytes;
	char *addr;
	char *optval;
	char *dir = "/disk1";
        //unsigned char buf[MAXBUFLEN];		// each byte contains 4 bit im + 4 bit real
	unsigned int affinity=0, core=1;	// CPU core for affinity
	unsigned long long index=0;
	char shmfile2[50];
	char shmfile[50];
	char datafile[50];
	unsigned int bindex;
  	time_t t = time(NULL);
  	struct tm file_tm;
        mqd_t qd_server;   // queue descriptors
	char* diobuf;
	ssize_t bytes_read, bytes_written, bytes_remaining;

        unsigned int fpga = 0;	// fpga: 0, 1, 2, 3
        typedef struct {
                int fpga;
                int index;
		int beamid;
        } queue_s;

        queue_s mqueue;



	if (setpriority(PRIO_PROCESS, 0, -20) != 0) {
        	syslog(LOG_INFO, "setpriority");
        	exit(0);
    	}


	// define signal
	signal(SIGINT, sigroutine);
	signal(SIGALRM, watchdog);	// set packet receiving timeout
	signal(SIGUSR1, low_data_rate); // set low data rate alarm
	signal(SIGUSR2, mq_error); // message queue error

        for(i=1;i<argc; i++) {
                if(strcmp(argv[i], "-i")==0) optval=argv[i+1];
                if(strcmp(argv[i], "-o")==0) dir=argv[i+1];
                if(strcmp(argv[i], "-ps")==0) packet_size=atol(argv[i+1]);
                if(strcmp(argv[i], "-pn")==0) packet_number=atol(argv[i+1]); 
                if(strcmp(argv[i], "-cpu")==0) {core=atoi(argv[i+1]); affinity=1;}
		if(strcmp(argv[i], "-port")==0) port=atoi(argv[i+1]);
		if(strcmp(argv[i], "-block")==0) block=atoi(argv[i+1]);
		if(strcmp(argv[i], "-fpga")==0) fpga=atoi(argv[i+1]);   // use 0,1,2,....

        }

	unsigned int bitmap_size = (block * packet_number / 8);
	unsigned long long block_t_packet_number = block*packet_number;
	size_t bufsize = packet_size*sizeof(unsigned char);
	size_t filesize = (block*packet_size*packet_number+bitmap_size)*sizeof(unsigned char);
        sprintf(shmfile, "/mnt/%s/fpga.bin", optval);
        int fd = open(shmfile, O_CREAT | O_RDWR, 0666);
        if(fd<0) exit(0);

        ftruncate(fd, filesize);
	syslog(LOG_INFO, "Allocating %ld bytes", filesize);




        // to attach the bitmap file (which contains which packets are received) to the end of packets received
        void *p = mmap(NULL, filesize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_HUGETLB | MAP_HUGE_1GB, fd, 0);
        if (p == MAP_FAILED)    {
                printf("Hugepage Mapping Failed\n");
                syslog(LOG_INFO, "Hugepage Mapping Failed");
                exit(0);
        }

	void *buf = mmap(NULL, bufsize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
	void *bitmap = mmap(NULL, block*packet_number*sizeof(unsigned char), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
        block_size = packet_number * packet_size;

	int ret = set_low_latfency();

	// prepare for shared memory and mmap

	int datafd=0;
	unsigned char *dest;

	void *pstart = p;
	void *pend = p + block * packet_size * packet_number * sizeof(unsigned char);
	void *bstart = buf;
	void *bitmapstart = bitmap;

	memset(p, 0, block*packet_number*packet_size*sizeof(unsigned char));
	memset(bitmap, 1, bitmap_size*sizeof(unsigned char));
        // end of shared memory
	
        // define message queue
        struct mq_attr attr;
        attr.mq_flags = 0;
        attr.mq_maxmsg = MAX_MESSAGES;
        attr.mq_msgsize = MAX_MSG_SIZE;
        attr.mq_curmsgs = 0;

    	if ((qd_server = mq_open (SERVER_QUEUE_NAME, O_RDWR | O_CREAT | O_NONBLOCK, QUEUE_PERMISSIONS, &attr)) == -1) {
        	perror ("Server: mq_open (server) error");
        	exit (1);
    	}
        // end of message queue setup


	// setup CPU affinity
	if (affinity) {
        	cpu_set_t  mask;
        	CPU_ZERO (&mask);
		CPU_SET(core, &mask);
		int result = sched_setaffinity(0, sizeof(mask), &mask);
		if (result<0) {
                	perror("CPU affinity");
                	exit(1);
		}
	}
	// end of CPU affinity
	

	// set up shared memory
        if ((shmid = shmget(SHM_ADD, sizeof(struct shmseg), 0666)) < 0) {
		syslog(LOG_INFO, "shmget failed");
                perror("shmget");
                exit(1);
        }

        if ((shmp = shmat(shmid, NULL, 0)) == (void*) - 1) {
		syslog(LOG_INFO, "shmgat failed");
                perror("shmat");
                exit(1);
        }


	// socket for FPGA packets
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		syslog(LOG_INFO, "socket error");
                perror("socket");
                exit(1);
        }

	// set up nonblock
	fcntl(sockfd, F_SETFL, O_NONBLOCK);

        // assign NIC
        if ((setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, optval, strlen(optval)))<0) {
                perror("setsockopt bind device");
		syslog(LOG_INFO, "setsockopt bind device error");
                exit(1);
        }

        //int rcvsize = 805306368;
        int rcvsize = 512*1024*1024;    // no receiver buffer error on kolong 
        if ((setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (char *)&rcvsize, (int)sizeof(rcvsize))) < 0)  {
		syslog(LOG_INFO, "rcvsize error");
                perror("rcvsize");
                exit(1);
        }
	
	int disable = 1;
	if ((setsockopt(sockfd, SOL_SOCKET, SO_NO_CHECK, (void*)&disable, sizeof(disable))) < 0) {
    		perror("setsockopt failed");
		syslog(LOG_INFO, "setsockopt failed");
	}	

	// set receive timeout to 2 sec
	struct timeval read_timeout;
	read_timeout.tv_sec = 2;
	read_timeout.tv_usec = 0;
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof read_timeout);

        server_addr.sin_family = AF_INET;         // host byte order
        server_addr.sin_port = htons(MYPORT);     // short, network byte order
        server_addr.sin_addr.s_addr = INADDR_ANY; // automatically fill with my IP
	memset(&(server_addr.sin_zero), '\0', 8); // zero the rest of the struct

        if ((bind(sockfd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr))) == -1) {
                perror("bind");
		syslog(LOG_INFO, "socket binding error");
                exit(1);
        }

        addr_len = sizeof(struct sockaddr);

	uint64_t t0, t1, t_start, t0_block;
       	unsigned char packet_order;
	*(&shmp->command_0+fpga) = status;
	//usleep(1);
	unsigned long long index_counter=1;	// index_counter increases 1 for every packet_number;
	unsigned int block_packet_number = block*packet_number;
	float dr=0;
	unsigned int low_rate=0, dr_low_limit=6.2;
	printf("Waiting for sendsocket command 0:stop 1:start 2:stop/save -1:exit\n");

	// test clear and copy bitmap to the end of pointer p
	memset(bitmap, 0x00, bitmap_size*sizeof(unsigned char));
	memcpy(pend, bitmap, bitmap_size*sizeof(unsigned char));


	while (status>=0 && status<3) {
		alarm(0);	// so when status = 0 is set, the watchdog will not timeout
		status = *(&shmp->command_0+fpga);
		*(&shmp->cpu_0+fpga) = core;
		*(&shmp->block_0+fpga) = block;
		sprintf(shmp->eth_0+fpga*15,"%s", optval);
		if(status==0) *(&shmp->dr_0+fpga) = 0.0;

                if ((numbytes=recvfrom(sockfd, buf, packet_size , 0, NULL, NULL)) == packet_size) {
                	// index counter has 40 bit
                        // 2^40 / 10^6 * 1.27 / 86400 ~ 16 days
                        memcpy(&t0, buf, 5*sizeof(unsigned char));
			memcpy(&packet_order, buf+8, 1*sizeof(unsigned char));
                        index_counter = t0/packet_number;
                }


		// should i wait for xxxxxx0 to start, so all fpgas will be synchronized??
		//
		if (status==1 && packet_order==1) {

			for (bindex=0; bindex < block; bindex++) {
				alarm(2);	// if the for loop does not come to here within 2 seconds, the alarm will be set off, or use ualarm()
				z = 0;
				loop = true;
				first = true;
				// clear bitmap mask of current block
				memset(bitmap+(bindex*(packet_number/8)*sizeof(unsigned char)), 0, (packet_number/8)*sizeof(unsigned char));

                        	start = clock();
				
				p = pstart + bindex * block_size * sizeof(unsigned char);
			
				while(loop) 
					{	
						if ((numbytes=recvfrom(sockfd, buf, packet_size , 0, NULL, NULL)) == packet_size) {
							// index counter has 40 bit
							// 2^40 / 10^6 * 1.27 / 86400 ~ 16 days
							memcpy(&t0, buf, 5*sizeof(unsigned char));
							if (first==true) {
								t_start = t0;
								t1 = t0 + packet_number - 1;
								if (bindex==0) t0_block = t0;
								first=false;
							}

							// deal with 1when index counter resets 
							if (t0>0 && t0<=1) {printf("reset\n"); index_counter=0; alarm(2);} // may increase to alarm(3) for testing

							p = pstart + ((t0-t0_block)%(block_t_packet_number))*packet_size * sizeof(unsigned char);
							memcpy(p, buf, packet_size);
							setBit(bitmap, (t0+bindex*packet_number-t_start)%(block_t_packet_number)); // block_t_packet_number = block*packet_number

                                                        if (t0 >= t1) {
                                                                loop=false;
                                                        }
							z++;
						}

					}
				index_counter++;
				end = clock();	

				dr = (packet_size*z/1e9)/(((double) (end-start))/CLOCKS_PER_SEC);
				*(&shmp->dr_0+fpga) = dr;

			
      				memcpy(pend, bitmapstart, bitmap_size); 	// if the computer is fast enough, then memcpy them all

				// need to take care of clearing the bitmask map
				status = *(&shmp->command_0+fpga);
                		//usleep(1);
				if (status!=1) break;

                                // send out message queue
                                mqueue.fpga=fpga;
                                mqueue.index=bindex;
				mqueue.beamid=bindex%16;
					//mqueue.beamid=-1;
				mq_send (qd_server, (const char *) &mqueue, sizeof(mqueue), 0);
                                // message queue sent

			} // bindex
		} // if status

		if (status==2) {

			alarm(0);
			time(&t);
			file_tm = *localtime(&t);
			*(&shmp->dr_0+fpga) = 0.0;
			// if the OUT_DIR="/disk1/fpga/"
			sprintf(datafile, "%sfpga%d.%02d%02d%02d%02d%02d.bin", OUT_DIR, fpga, file_tm.tm_mon + 1, file_tm.tm_mday, file_tm.tm_hour, file_tm.tm_min, file_tm.tm_sec);
			// if the OUT_DIR="/disk" then
			//sprintf(datafile, "%s%d/fpga%d.%02d%02d%02d%02d%02d.bin", dir, fpga, fpga, file_tm.tm_mon + 1, file_tm.tm_mday, file_tm.tm_hour, file_tm.tm_min, file_tm.tm_sec);
        		datafd = open(datafile, O_CREAT | O_WRONLY | O_TRUNC | O_DIRECT, 0666);
			if(datafd) {
				posix_fallocate(datafd, 0, filesize);	// allocate consecutive buffer
				lseek(fd, 0, SEEK_SET);   
				if (posix_memalign((void**)&diobuf, getpagesize(), DIO_BUFFER_SIZE) != 0) {
                        		syslog(LOG_INFO,"Buffer allocation error");
					close(datafd);
				}

                		// Copy data using direct I/O
                		bytes_remaining = filesize;
                		while ((bytes_read = read(fd, diobuf, DIO_BUFFER_SIZE)) > 0) {
                        		bytes_written = write(datafd, diobuf, bytes_read);
                                	bytes_remaining = bytes_remaining - bytes_written;
                                	if (bytes_written == -1) {
                                        	syslog(LOG_INFO, "Direct I/O error");
                                        	break;
                                	}
                                	if (bytes_remaining<=0) {
                                        	syslog(LOG_INFO, "Done writing %s", datafile);
                                        	break;
                                	}

                		}

        			p = pstart;
				close(datafd);

				*(&shmp->command_0+fpga) = 1;

			}
			if(datafd<0) {
				syslog(LOG_INFO, "Output file creation error");
				status = -1;
				*(&shmp->command_0+fpga) = -1;
			}

		}


	} // while status

	*(&shmp->dr_0+fpga) = 0.0;
	*(&shmp->block_0+fpga) = 0;

        if (shmdt(shmp) == -1) {
                fprintf(stderr, "shmdt failed\n");
                exit(EXIT_FAILURE);
        }

        munmap(p, filesize);
        munmap(buf, bufsize);
        munmap(bitmap, block*bitmap_size);

        posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);

	close(sockfd);
	close(fd);

        return 0;
}

