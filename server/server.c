#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>

int running = 1;
int fd;
void *map_base;
const off_t map_start_addr = 0x7C440000; // needs to be page aligned
const unsigned map_width = 0x10000; // needs to be page aligned
// fifo core is at 0x 7C 44 00 00
// pss core is at  0x 7C 44 40 00
// rx core is at   0x 7C 44 80 00
int sockfd, newsockfd, portno;
unsigned page_size, mapped_size;
int debug_level = 0;

void handle_sigint(int sig)
{
    running = 0;
}

void print_debug(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    if (debug_level > 0)
        vprintf(format, args);
    va_end(args);
}

void handle_rx_msg(char *msg, uint32_t len)
{
    void *virt_addr;
    char *reply_msg = (char*) malloc(len);
    switch(msg[0])
    {
        case 0:
            // read memory
            reply_msg[0] = 0;
            for (int i = 0; i < (len - 1)/4; i++)
            {
                unsigned read_addr = *(uint32_t*)(msg + 1 + i*4);
                virt_addr = map_base + read_addr - (unsigned)map_start_addr;
                uint32_t mem_data = *(volatile uint32_t*)virt_addr;
                *(uint32_t*)(reply_msg + 1 + i*4) = mem_data;
                print_debug("reading from addr %08x -> %08x\n", read_addr, mem_data);
            }
            // send reply
            print_debug("sending reply: ");
            for (int i = 0; i < len; i++)  print_debug("%02x ", reply_msg[i]);
            print_debug("\n");
            if (send(newsockfd, &len, sizeof(len), 0) == -1) {
                perror("send");
                exit(EXIT_FAILURE);
            }

            if (send(newsockfd, reply_msg, len, 0) == -1) {
                perror("send");
                exit(EXIT_FAILURE);
            }
        break;
        case 1:
            // write memory
            for (int i = 0; i < (len - 1)/8; i++)
            {
                unsigned write_addr = *(uint32_t*)(msg + 1 + i*8);
                virt_addr = map_base + write_addr - (unsigned)map_start_addr;
                uint32_t mem_data = *(uint32_t*)(msg + 1 + 4 + i*8);
                print_debug("write %08x to addr %08x\n", mem_data, mem_data);
                *(volatile uint32_t*)virt_addr = mem_data;
            }
        break;
        default:
            printf("ERROR: unknown message type!");
        break;
    }
    free(reply_msg);    
}

void map_memory()
{
	fd = open("/dev/mem", O_RDWR | O_SYNC);
	mapped_size = page_size = getpagesize();
    // unsigned offset_in_page = (unsigned)map_start_addr & (page_size - 1);
    // mapped_size = ceil(map_width/page_size)
    mapped_size = map_width;
	map_base = mmap(NULL,
			mapped_size,
			PROT_READ | PROT_WRITE,
			MAP_SHARED,
			fd,
			map_start_addr);
	if (map_base == MAP_FAILED)
        perror("ERROR mmap");
    printf("page size = %08x, map_base = %p\n", page_size, map_base);
}

void unmap_memory(){
    if (munmap(map_base, mapped_size) == -1)
        perror("ERROR munmap");
    close(fd);    
}

int main(int argc, char *argv[]) {
    unsigned int clilen;
    struct sockaddr_in serv_addr, cli_addr;
    int n;

    if (argc > 1 && strncmp(argv[1], "-d", 2))
        debug_level = 1;
    else
        debug_level = 0;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(1);
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = 69;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *) &serv_addr,
             sizeof(serv_addr)) < 0) {
        perror("ERROR on binding");
        exit(1);
    }

    printf("mapping memory...\n");
    map_memory();
    printf("memory mapped\n");

    // Set up SIGINT signal handler
    struct sigaction a;
    a.sa_handler = handle_sigint;
    a.sa_flags = 0;
    sigemptyset( &a.sa_mask );
    sigaction( SIGINT, &a, NULL );

    listen(sockfd,5);
    printf("server running on port %d\n", portno);

    unsigned alloc_size = 1024;
    char* recv_data = (char*) malloc(alloc_size);
    while(running)
    {

        clilen = sizeof(cli_addr);
        printf("waiting for client...\n");
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd < 0) {
            perror("ERROR on accept");
            exit(1);
        }
        printf("client connected\n");

        uint32_t recv_data_len;
        while (running) {
            if ((n = recv(newsockfd, &recv_data_len, sizeof(recv_data_len), MSG_WAITALL)) == -1) {
                perror("recv");
                exit(EXIT_FAILURE);
            }

            if (recv_data_len > alloc_size)
            {
                // realloc buffer if recv size is larger than allocated memory
                free(recv_data);
                alloc_size = recv_data_len;
                char* recv_data = (char*) malloc(alloc_size);
            }

            if ((n = recv(newsockfd, recv_data, recv_data_len, MSG_WAITALL)) == -1) {
                perror("recv");
                exit(EXIT_FAILURE);
            }

            if (n == 0)
            {
                printf("Client disconnected\n");
                break;
            }

            if (recv_data_len != 0)
            {
                print_debug("Received %d bytes: ", recv_data_len);
                for (unsigned int i = 0; i < recv_data_len; i++)
                {
                    print_debug("%02x ", recv_data[i]);
                }
                print_debug("\n");
                handle_rx_msg(recv_data, recv_data_len);
            }
        }
    }

    close(newsockfd);
    close(sockfd);
    // unmap_memory();
    free(recv_data);

    return 0;
}
