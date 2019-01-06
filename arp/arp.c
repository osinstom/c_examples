//
// Created by autonet on 06.01.19.
//

#include <stdio.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#include <arpa/inet.h>  //htons etc

#define ETH_HDR_LEN 14
#define MAC_LENGTH 6
#define IPV4_LENGTH 4
#define BUF_SIZE 60
#define ARP_REQUEST_TYPE 0x01
#define ARP_RESPONSE_TYPE 0x02
#define HW_TYPE 1
#define PROTO_ARP 0x0806

struct arp_hdr {
    unsigned short hardware_type;
    unsigned short protocol_type;
    unsigned char  hardware_len;
    unsigned char  protocol_len;
    unsigned short opcode;
    unsigned char sender_mac[MAC_LENGTH];
    unsigned char sender_ip[IPV4_LENGTH];
    unsigned char target_mac[MAC_LENGTH];
    unsigned char target_ip[IPV4_LENGTH];
};

void get_if_info(int *sockfd, const char *ifname, char *mac, char* ip, int *ifindex)
{
    struct ifreq ifr;
    strcpy(ifr.ifr_name, ifname);
    ioctl(*sockfd, SIOCGIFINDEX, &ifr);
    *ifindex = ifr.ifr_ifindex;
    printf("interface index is %d\n", *ifindex);

    if (ioctl(*sockfd, SIOCGIFHWADDR, &ifr) == -1)
    {
        printf("Get MAC of interface %s failed!\n", ifname);
        return;
    }
    // save MAC address in mac variable
    memcpy(mac, ifr.ifr_hwaddr.sa_data, MAC_LENGTH);

    ioctl(*sockfd, SIOCGIFADDR, &ifr);
    struct sockaddr_in *i = (struct sockaddr_in *) &ifr.ifr_addr;
    ip = inet_ntoa(i->sin_addr);

}

int send_arp_req(int sockfd, int ifindex, char* mac, char* src_ip, const char* dst_ip)
{
    unsigned char sendbuffer[BUF_SIZE];
    memset(sendbuffer, 0, sizeof(sendbuffer));

    struct sockaddr_ll socket_address;
    socket_address.sll_family = AF_PACKET;
    socket_address.sll_protocol = htons(ETH_P_ARP);
    socket_address.sll_ifindex = ifindex;
    socket_address.sll_hatype = htons(ARPHRD_ETHER);
    socket_address.sll_pkttype = (PACKET_BROADCAST);
    socket_address.sll_halen = MAC_LENGTH;
    socket_address.sll_addr[6] = 0x00;
    socket_address.sll_addr[7] = 0x00;

    struct ethhdr *send_req = (struct ethhdr *) sendbuffer;
    struct arp_hdr *arp_req = (struct arp_hdr *) (sendbuffer + ETH_HDR_LEN);

    // set destination MAC address to BROADCAST
    memset(send_req->h_dest, 0xff, MAC_LENGTH);

    // set target MAC of ARP header to zero
    memset(arp_req->target_mac, 0x00, MAC_LENGTH);

    //Set source MAC to our MAC address
    memcpy(send_req->h_source, mac, MAC_LENGTH);
    memcpy(arp_req->sender_mac, mac, MAC_LENGTH);
    memcpy(socket_address.sll_addr, mac, MAC_LENGTH);

    /* Setting protocol of the packet */
    send_req->h_proto = htons(ETH_P_ARP);

    /* Creating ARP request */
    arp_req->hardware_type = htons(HW_TYPE);
    arp_req->protocol_type = htons(ETH_P_IP);
    arp_req->hardware_len = MAC_LENGTH;
    arp_req->protocol_len = IPV4_LENGTH;
    arp_req->opcode = htons(ARP_REQUEST_TYPE);

    uint32_t src = inet_addr(src_ip);
    uint32_t dst = inet_addr(dst_ip);

    memcpy(arp_req->sender_ip, &src, sizeof(uint32_t));
    memcpy(arp_req->target_ip, &dst, sizeof(uint32_t));

    int ret = sendto(sockfd, sendbuffer, 42, 0,
            (struct sockaddr *) &socket_address, sizeof(socket_address));

    return ret;
}

int recv_arp(int sockfd, char* dst_mac)
{
    char recvbuffer[BUF_SIZE];
    ssize_t length = recvfrom(sockfd, recvbuffer, BUF_SIZE, 0, NULL, NULL);
    printf("Received packet length: %ld\n", length);
    struct ethhdr *resp = (struct ethhdr *) recvbuffer;
    struct arp_hdr *arp_resp = (struct arp_hdr *) (recvbuffer + ETH_HDR_LEN);
    if (ntohs(resp->h_proto) != PROTO_ARP) {
        printf("Got not an ARP packet\n");
        return;
    }

    if (ntohs(arp_resp->opcode) != ARP_RESPONSE_TYPE)
    {
        printf("Received packet is not an ARP reply\n");
        return;
    }

    memcpy(dst_mac, &arp_resp->sender_mac, sizeof(arp_resp->sender_mac));

    return 0;
}

void send_arp(const char *ifname, const char *dst_ip, char *dst_mac)
{
    int sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ARP));

    int ifindex;
    char mac[MAC_LENGTH];
    char src_ip[IPV4_LENGTH];
    get_if_info(&sockfd, ifname, mac, src_ip, &ifindex);

    struct sockaddr_ll saddrll;
    memset(&saddrll, 0, sizeof(struct sockaddr_ll));
    saddrll.sll_family = AF_PACKET;
    saddrll.sll_ifindex = ifindex;

    if (bind(sockfd, (struct sockaddr*) &saddrll, sizeof(struct sockaddr_ll)) < 0)
    {
        printf("Bind failed!\n");
        return;
    }

    if (send_arp_req(sockfd, ifindex, mac, src_ip, dst_ip) == -1)
    {
       printf("Send ARP request failed!\n");
       return;
    }

    while(1) {
        int got_reply = recv_arp(sockfd, dst_mac);
        if (got_reply == 0)
            break;
    }

    if (sockfd)
        close(sockfd);
}

int main(int argc, char* argv[])
{
    if (argc != 3) {
        printf("Usage: %s <INTERFACE> <DEST_IP>\n", argv[0]);
        return 1;
    }
    const char *ifname = argv[1];
    const char *ip = argv[2];

    char dst_mac[MAC_LENGTH];
    memset(dst_mac, 0, sizeof(dst_mac));
    send_arp(ifname, ip, dst_mac);
    printf("MAC address of %s is %02X:%02X:%02X:%02X:%02X:%02X\n", ip,
            dst_mac[0],
            dst_mac[1],
            dst_mac[2],
            dst_mac[3],
            dst_mac[4],
            dst_mac[5]);
}