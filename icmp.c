//
// Created by fuji on 18-4-30.
//

#include <netinet/ip.h>
#include <netinet/ip_icmp.h>

#include "icmp.h"
#include "inet.h"
#include "ip.h"

int handle_icmp_echo(int sockfd, void *buffer) {
    struct ip *iph = buffer;
    unsigned int ip_hl = iph->ip_hl * 4;
    struct icmphdr *icmph = (struct icmphdr *) ((char *) iph + ip_hl);

    // first, save args in iph: reverse src & dst
    struct in_addr ip_src = iph->ip_dst;
    struct in_addr ip_dst = iph->ip_src;
    unsigned int data_len = ntohs(iph->ip_len) - ip_hl;

    // shrink buffer begin pos
    iph = buffer = (char *) iph + (ip_hl - sizeof(struct ip));

    ip_build_header(iph, ip_src, ip_dst, IPPROTO_ICMP, data_len);
    ip_hl = iph->ip_hl * 4;

    // modify icmp header (not rebuild)
    icmph->type = ICMP_ECHOREPLY;
    icmph->code = 0;
    icmph->checksum = 0;
    // left id && sequence unchanged
    // calculate icmp checksum
    icmph->checksum = inet_cksum((unsigned short *) icmph, data_len, 0);

    // send this packet out
    return ip_send(sockfd, buffer, ip_hl + data_len);
}

int handle_icmp(int sockfd, void *buffer, size_t nbytes) {
    struct ip *iph = buffer;
    unsigned int ip_hl = iph->ip_hl * 4;

    if (nbytes < ip_hl + sizeof(struct icmphdr))
        return -1;
    struct icmphdr *icmph = (struct icmphdr *) ((char *) iph + ip_hl);
    // TODO we can check icmp checksum here

    switch (icmph->type) {
        case ICMP_ECHO:
            handle_icmp_echo(sockfd, buffer);
            break;
        default:
            break;
    }
    return 0;
}
