//
// Created by fuji on 18-4-30.
//

#include <stdio.h>

#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>

#include "icmp.h"
#include "inet.h"
#include "ip.h"

static int handle_icmp_echo(int sockfd, void *buffer) {
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

static int handle_icmp_echo_reply(int sockfd, void *buffer) {
    struct ip *iph = buffer;
    unsigned int ip_hl = iph->ip_hl * 4;
    struct icmphdr *icmph = (struct icmphdr *) ((char *) iph + ip_hl);

    printf("%hu bytes from %s: icmp_id=%hu icmp_seq=%hu ttl=%d\n",
           ntohs(iph->ip_len), inet_ntoa(iph->ip_src),
           ntohs(icmph->un.echo.id),
           ntohs(icmph->un.echo.sequence), iph->ip_ttl);
    return 0;
}

int handle_icmp(int sockfd, void *buffer, size_t nbytes) {
    struct ip *iph = buffer;
    unsigned int ip_hl = iph->ip_hl * 4;

    if (nbytes < ip_hl + sizeof(struct icmphdr))
        return -1;
    struct icmphdr *icmph = (struct icmphdr *) ((char *) iph + ip_hl);
    // check icmp checksum
    unsigned short sum = inet_cksum((unsigned short *) icmph, ntohs(iph->ip_len) - ip_hl, 0);
    if (sum != 0) {
        fprintf(stderr, "handle_icmp: bad packet\n");
        return -1;
    }

    switch (icmph->type) {
        case ICMP_ECHO:
            handle_icmp_echo(sockfd, buffer);
            break;
        case ICMP_ECHOREPLY:
            handle_icmp_echo_reply(sockfd, buffer);
            break;
        default:
            break;
    }
    return 0;
}
