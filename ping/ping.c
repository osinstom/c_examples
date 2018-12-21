#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/ip_icmp.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>

#define PING_SLEEP_RATE 1000000

// define pacet size, TODO: enable variable size
#define PING_PACKET_SIZE 64

// timeout for packets in seconds
#define RECV_TIMEOUT 1

int loop = 1;

struct ping_hdr {
    struct icmphdr hdr;
    char msg[PING_PACKET_SIZE - sizeof(struct icmphdr)];
};

char* do_dns_lookup(char *hostname, struct sockaddr_in *addr)
{
    printf("\nResolving DNS..\n");
    struct hostent *host_entity;

    char *ip = (char*) malloc(NI_MAXHOST*sizeof(char));

    host_entity = gethostbyname(hostname);
    if (host_entity == NULL) {
        return NULL;
    }

    strcpy(ip, inet_ntoa(*(struct in_addr *) host_entity->h_addr_list[0]));

    (*addr).sin_family = host_entity->h_addrtype;
    (*addr).sin_port = htons(0);
    (*addr).sin_addr.s_addr = *(long*)host_entity->h_addr_list[0];

    return ip;
};

void interruptHandler() {
    loop = 0;
}

unsigned short checksum(void *b, int len) {
    unsigned short *buf = b;
    unsigned int sum = 0;
    unsigned short result;

    for ( sum = 0; len > 1; len -= 2 )
        sum += *buf++;
    if ( len == 1 )
        sum += *(unsigned char*)buf;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;

    return result;
}

void print_summary(char* ip_addr, int msg_count, int msg_received_count, long double total_msec) {
    printf("\n===%s ping statistics===\n", ip_addr);
    printf("\n%d packets sent, %d packets received, %f percent"
           " packet loss. Total time: %Lf ms.\n\n",
           msg_count, msg_received_count,
           ((msg_count - msg_received_count)/msg_count) * 100.0,
           total_msec);
}

void send_ping(int sockfd, struct sockaddr_in *ping_addr, char* ip_addr, char* hostname) {

    int ttl = 64;
    struct sockaddr_in *recv_sockaddr;

    int sockopt = setsockopt(sockfd, SOL_IP, IP_TTL, &ttl, sizeof(ttl));

    if (sockopt != 0) {
        printf("\nSetting socket options to TTL failed!\n");
        return;
    }

    //timers
    struct timespec time_start, time_end, tfs, tfe;
    struct timeval tv_out;
    tv_out.tv_sec = RECV_TIMEOUT;
    tv_out.tv_usec = 0;

    clock_gettime(CLOCK_MONOTONIC, &tfs);

    // setting timeout of recv setting
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,
               (const char*)&tv_out, sizeof tv_out);

    // counters
    int msg_count=0, msg_received_count=0;

    while(loop) {
        struct ping_hdr pkt;
        bzero(&pkt, sizeof(pkt));
        pkt.hdr.type = ICMP_ECHO;
        pkt.hdr.un.echo.id = getpid();

        // fill ICMP payload with random bits
        int i;
        for (i=0; i < sizeof(pkt.msg)-1; i++)
            pkt.msg[i] = i+'0';

        pkt.msg[i] = 0;
        pkt.hdr.un.echo.sequence = msg_count++;
        pkt.hdr.checksum = checksum(&pkt, sizeof(pkt));

        usleep(PING_SLEEP_RATE);
        printf("Packet type: %d\n", pkt.hdr.type);
        //send packet
        clock_gettime(CLOCK_MONOTONIC, &time_start);
        int sent = sendto(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr*) ping_addr,
                sizeof(*ping_addr));

        if (sent <= 0)
        {
            printf("\nSending packet failed!\n");
        }

        int addr_len = sizeof(*recv_sockaddr);
        bzero(&pkt, sizeof(pkt));
        printf("Packet type: %d\n", pkt.hdr.type);
        int recv = recvfrom(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr*) recv_sockaddr,
                &addr_len);

        if (recv <= 0)
        {
            printf("\nReceive packet failed!\n");
        } else
        {
            clock_gettime(CLOCK_MONOTONIC, &time_end);
            double rtt = ((double) time_end.tv_nsec - time_start.tv_nsec)/1000000.0;

            printf("%d bytes from %s (%s) seq=%d ttl=%d "
                   "rtt = %f ms.\n",
                   PING_PACKET_SIZE, hostname,
                    ip_addr, msg_count,
                    ttl, rtt);
            printf("Packet type: %d\n", pkt.hdr.type);

            msg_received_count++;


        }

    }

    clock_gettime(CLOCK_MONOTONIC, &tfe);
    double timeElapsed = ((double)(tfe.tv_nsec -
                                   tfs.tv_nsec))/1000000.0;

    long double total_msec = (tfe.tv_sec-tfs.tv_sec)*1000.0+
                 timeElapsed;

    print_summary(ip_addr, msg_count, msg_received_count, total_msec);

}


int main(int argc, char *argv[]) {

    int sockfd;
    struct sockaddr_in addr_conn;

    if (argc!=2) {
        printf("\nYou need to type destination IP address\n");
        return 0;
    }

    char *addr = argv[1];
    char* ip_addr = do_dns_lookup(addr, &addr_conn);

    if(ip_addr==NULL)
    {
        printf("\nDNS lookup failed! Could not resolve hostname!\n");
        return 0;
    }

    printf("IP Address: %s\n", ip_addr);

    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);

    signal(SIGINT, interruptHandler); //catching interrupt

    send_ping(sockfd, &addr_conn, ip_addr, addr);

    return 0;
}