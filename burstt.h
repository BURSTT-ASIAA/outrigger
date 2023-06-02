#define _GNU_SOURCE
#include <x86intrin.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>       /* For O_* constants */
#include <sys/mman.h>
#include <asm/mman.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include <syslog.h>
#include <mqueue.h>
#include <sched.h>
#include <immintrin.h>
#include <assert.h>
#include <numa.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/file.h>
#include <sys/sysinfo.h>
#include <limits.h>

// data out directory
// when there is only one disk
#define OUT_DIR "/disk1/fpga/"
//or when there are 4 disks for each fpga
// #define OUT_DIR "/disk"


// NIC

#define MYPORT 60000    // the port users will be connecting to
#define MAXBUFLEN 9000
#define MAP_HUGE_1GB    (30 << MAP_HUGE_SHIFT)
#define ADDR (void *) (0x0UL)   //starting address of the page


// message queue: /dev/mqueue/burstt
#define SERVER_QUEUE_NAME   "/burstt"
#define QUEUE_PERMISSIONS 0660
#define MAX_MESSAGES 20
#define MAX_MSG_SIZE 256
#define MSG_BUFFER_SIZE MAX_MSG_SIZE + 10


// shared memory & socket port
#define SHM_ADD 0x1010
#define SOCK_PORT 8000    // the port users will be connecting to


/* bit clear: 
  a: int,
  pos: bit position to clear */
#define CLEARBIT(a, pos) (a &= ~(1 << pos))

// buffer size for direct i/o, same as disk block size from # blockdev --getbsz /dev/sda1
#define DIO_BUFFER_SIZE 4096

// Global flag to indicate if the daemon should keep running
volatile sig_atomic_t keepRunning = true;


// shared memory structure
struct shmseg {
        int command_0;
        int command_1;
        int command_2;
        int command_3;
        int page_0;
        int page_1;
        int page_2;
        int page_3;
        int cpu_0;
        int cpu_1;
        int cpu_2;
        int cpu_3;
        int block_0;
        int block_1;
        int block_2;
        int block_3;
        char eth_0[15];
        char eth_1[15];
        char eth_2[15];
        char eth_3[15]; 
        float dr_0;
        float dr_1;
        float dr_2;
        float dr_3;
        int err_0;
        int err_1;
        int err_2;
        int err_3;
        };


struct shmseg_t {
        unsigned long t_seconds;
        int command_0;
        int command_1;
        int command_2;
        int command_3;
        int page_0;
        int page_1;
        int page_2;
        int page_3;
        int cpu_0;
        int cpu_1;
        int cpu_2;
        int cpu_3;
        int block_0;
        int block_1;
        int block_2;
        int block_3;
        char eth_0[15];
        char eth_1[15];
        char eth_2[15];
        char eth_3[15];
        float dr_0;
        float dr_1;
        float dr_2;
        float dr_3;
        int err_0;
        int err_1;
        int err_2;
        int err_3;
        };

