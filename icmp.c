//
// Created by fuji on 18-4-30.
//

#include <netinet/ip.h>
#include <netinet/ip_icmp.h>

#include "icmp.h"
#include "router.h"

int reply_icmp_echo(int sockfd, void *buffer, size_t nbytes){
    struct iphdr *iph = buffer;
    unsigned int iph_len = iph->ihl * 4;
    struct icmphdr *icmph = (struct icmphdr *) ((unsigned char *) iph + iph_len);

    // TODO build ip header(we only build a basic ip header
    // no matter what kind of header received)
    // we can do this by shrinking buffer begin pos.
    // so we should first save relative args in iph

    // TODO calculate ip header checksum
    // TODO build icmp header
    // TODO calculate icmp checksum
}

int process_icmp(int sockfd, void *buffer, size_t nbytes) {
    struct iphdr *iph = buffer;
    unsigned int iph_len = iph->ihl * 4;

    if(nbytes < iph_len + sizeof(struct icmphdr))
        return -1;
    struct icmphdr *icmph = (struct icmphdr *) ((unsigned char *) iph + iph_len);
    // TODO we can calculate icmp checksum here

    switch (icmph->type) {
        case ICMP_ECHO:
            reply_icmp_echo(sockfd, buffer, nbytes);
            break;
        default:
            break;
    }
    return 0;
}
