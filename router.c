//
// Created by fuji on 18-5-1.
//



#include <netinet/in.h>
#include <linux/if_ether.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "icmp.h"
#include "inet.h"
#include "ip.h"

#define BUF_LEN 2048

int receive(int sockfd, void *buffer, size_t nbytes) {
    // only process icmp packet for now
    struct ip *iph = buffer;

    if (nbytes < ntohs(iph->ip_len))
        return -1;
    // TODO we can check ip checksum here

    switch (iph->ip_p) {
        case IPPROTO_ICMP:
            handle_icmp(sockfd, buffer, nbytes);
            break;
        default:
            break;
    }
    return 0;
}

/**
 * route daemon
 */
void routed(void) {
    unsigned char buffer[BUF_LEN];

    int sockfd = socket(AF_PACKET, SOCK_DGRAM, htons(ETH_P_IP));
    if (sockfd < 0) {
        fprintf(stderr, "routed: socket: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    ssize_t nbytes;
    while ((nbytes = recv(sockfd, buffer, BUF_LEN, 0)) > 0) {
        if (nbytes < sizeof(struct ip))
            continue;
        // check if we should receive this packet
        struct ip *iph = (struct ip *) buffer;
        if (inet_lookup(iph->ip_src) == NULL)
            ip_forward(sockfd, buffer, (size_t) nbytes);
        else
            receive(sockfd, buffer, (size_t) nbytes);
    }
    if (close(sockfd) < 0)
        fprintf(stderr, "routed: close: %s\n", strerror(errno));
}

int main(int argc, char *argv[]){
    // TODO init interface list
    // add default route when init interface
    // TODO init route table
    // TODO start arp daemon
    // TODO start router daemon
    routed();
    return 0;
}
