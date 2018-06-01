//
// Created by fuji on 18-5-1.
//

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <unistd.h>

#include "arp.h"
#include "icmp.h"
#include "inet.h"
#include "ip.h"

#define BUF_LEN 256

void usage(void) {
    fprintf(stderr, "Usage: host interface route dest\n");
    exit(EXIT_FAILURE);
}

int send_icmp_echo(int sockfd, struct in_addr dest) {
    unsigned char buffer[BUF_LEN];
    struct ip *iph = (struct ip *) buffer;
    struct icmphdr *icmph = (struct icmphdr *) &iph[1];

    // Use dest addr to get gateway info
    struct ip_route_entry *ir_entry = ip_route_match(dest);
    if (ir_entry == NULL) {
        fprintf(stderr, "send_icmp_echo: no route for %s\n", inet_ntoa(dest));
        return -1;
    }
    // get source addr
    struct inet_entry *i_entry;
    if (ir_entry->gateway.s_addr != 0)
        i_entry = inet_match(ir_entry->gateway);
    else
        i_entry = inet_match(dest);

    if (i_entry == NULL) {
        fprintf(stderr, "send_icmp_echo: no match src for %s\n", inet_ntoa(dest));
        return -1;
    }

    ip_build_header(iph, i_entry->addr, dest, IPPROTO_ICMP, BUF_LEN - sizeof(struct ip));

    // build icmp header
    icmph->type = ICMP_ECHO;
    icmph->code = 0;
    icmph->checksum = 0;
    icmph->un.echo.sequence = htons(1);
    icmph->un.echo.id = htons((uint16_t) getpid());   /* ID */

    /* compute ICMP checksum here */
    icmph->checksum = inet_cksum((unsigned short *) icmph, BUF_LEN - sizeof(struct ip), 0);

    return ip_send(sockfd, buffer, BUF_LEN);
}

// build a simple icmp packet && send
int main(int argc, char *argv[]) {
    FILE *fp;
    int n;

    char if_str[IFNAMSIZ];
    char addr_str[16];
    char netmask_str[16];
    char gateway_str[16];

    if (argc < 4)
        usage();

    // setup inet interface
    if ((fp = fopen(argv[1], "r")) == NULL) {
        fprintf(stderr, "host: fopen: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    while ((n = fscanf(fp, "%s %15s %15s", if_str, addr_str, netmask_str)) == 3) {
        if (inet_add(addr_str, netmask_str, if_str) == NULL)
            exit(EXIT_FAILURE);
        // add default route for every inet_entry
        // 0.0.0.0 stands for eth out
        if (ip_route_add(addr_str, netmask_str, "0.0.0.0") == NULL)
            exit(EXIT_FAILURE);
    }
    if (n != EOF) {
        fprintf(stderr, "host: interface config error\n");
        exit(EXIT_FAILURE);
    }
    fclose(fp);

    // setup route table
    if ((fp = fopen(argv[2], "r")) == NULL) {
        fprintf(stderr, "host: fopen: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    while ((n = fscanf(fp, "%15s %15s %15s", addr_str, netmask_str, gateway_str)) == 3) {
        // add default route for this entry
        if (ip_route_add(addr_str, netmask_str, gateway_str) == NULL)
            exit(EXIT_FAILURE);
    }
    if (n != EOF) {
        fprintf(stderr, "host: route config error\n");
        exit(EXIT_FAILURE);
    }
    fclose(fp);

    pthread_t arpd_tid;
    pthread_t routed_tid;
    // start arp daemon
    if (pthread_create(&arpd_tid, NULL, arpd, NULL) != 0) {
        fprintf(stderr, "host: can't start arpd\n");
        exit(EXIT_FAILURE);
    }
    // start router daemon
    if (pthread_create(&routed_tid, NULL, ip_routed, handle_icmp) != 0) {
        fprintf(stderr, "host: can't start ip_routed\n");
        exit(EXIT_FAILURE);
    }

    int sockfd = socket(AF_PACKET, SOCK_DGRAM, htons(ETH_P_IP));
    if (sockfd < 0) {
        fprintf(stderr, "host: socket: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    struct in_addr dest;
    if (inet_aton(argv[3], &dest) == 0) {
        fprintf(stderr, "host: invalid dest %s\n", argv[3]);
        exit(EXIT_FAILURE);
    }

    send_icmp_echo(sockfd, dest);

    if (close(sockfd) < 0) {
        fprintf(stderr, "host: close: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    pthread_join(routed_tid, NULL);

    exit(EXIT_SUCCESS);
}
