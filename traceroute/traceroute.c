//
// Created by p4dev on 18.12.18.
//

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
#include <sys/time.h>
#include <sys/param.h>
#include <err.h>

#define PING_SLEEP_RATE 1000000

#define PING_PACKET_SIZE 64

#define WAIT_TIME 5

// timeout for packets in seconds
#define RECV_TIMEOUT 5

int loop = 1;

struct ping_hdr {
    struct icmphdr hdr;
    char msg[PING_PACKET_SIZE - sizeof(struct icmphdr)];
};

void do_traceroute(int sockfd, struct sockaddr_in *pIn, char *addr);

void stop() {
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

void do_traceroute(int sockfd, struct sockaddr_in *addr_conn, char *addr) {

    int ttl = 1;
    struct sockaddr_in *recv_sockaddr;

    int msg_count=0;

    struct timeval tv_out;
    tv_out.tv_sec = RECV_TIMEOUT;
    tv_out.tv_usec = 0;

    // setting timeout of recv setting
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,
               (const char*)&tv_out, sizeof tv_out);

    while(loop)
    {
        int sockopt = setsockopt(sockfd, SOL_IP, IP_TTL, &ttl, sizeof(ttl));
        if (sockopt != 0) {
            printf("\nSetting socket options to TTL failed!\n");
            return;
        }

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

        int sent = sendto(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr*) addr_conn, sizeof(*addr_conn));

        if (sent <= 0)
        {
            printf("\nSending packet failed!\n");
        } else {

            int addr_len = sizeof(*recv_sockaddr);
            int recv = recvfrom(sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr*) recv_sockaddr,
                                &addr_len);
            
            printf("Packet type: %d, Code= %d\n", pkt.hdr.type, pkt.hdr.code);

            ttl++;
        }


    }

}


int main(int argc, char *argv[])
{
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

    signal(SIGINT, stop); //catching interrupt

    do_traceroute(sockfd, &addr_conn, ip_addr);

}
