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
#include <sys/time.h>
#include <unistd.h>

#include "arp.h"
#include "icmp.h"
#include "inet.h"
#include "ip.h"

#define BUF_LEN 256

void usage(void) {
    fprintf(stderr, "Usage: host inet route src dest\n");
    exit(EXIT_FAILURE);
}

int send_icmp_echo(char *src_str, char *dest_str) {
    unsigned char buffer[BUF_LEN];
    struct ip *iph = (struct ip *) buffer;
    struct icmphdr *icmph = (struct icmphdr *) &iph[1];
    struct in_addr src, dest;

    if (inet_aton(src_str, &src) == 0) {
        fprintf(stderr, "send_icmp_echo: invalid src %s\n", src_str);
        return -1;
    }
    if (inet_aton(dest_str, &dest) == 0) {
        fprintf(stderr, "send_icmp_echo: invalid dest %s\n", dest_str);
        return -1;
    }
    ip_build_header(iph, src, dest, IPPROTO_ICMP, BUF_LEN - sizeof(struct ip));

    // build icmp header
    icmph->type = ICMP_ECHO;
    icmph->code = 0;
    icmph->checksum = 0;
    icmph->un.echo.sequence = htons(1);
    icmph->un.echo.id = htons((uint16_t) getpid());   /* ID */
    gettimeofday((struct timeval *) &icmph[1], NULL);

    /* compute ICMP checksum here */
    icmph->checksum = inet_cksum((unsigned short *) icmph, BUF_LEN-sizeof(struct ip), 0);

    int sockfd = socket(AF_PACKET, SOCK_DGRAM, htons(ETH_P_IP));
    if (sockfd < 0) {
        fprintf(stderr, "send_icmp_echo: socket: %s\n", strerror(errno));
        return -1;
    }

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

    if (argc < 5)
        usage();

    // init inet list
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

    // TODO init route table
    if ((fp = fopen(argv[1], "r")) == NULL) {
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
    // start arp daemon
    if (pthread_create(&arpd_tid, NULL, arpd, NULL) != 0) {
        fprintf(stderr, "host: can't start arpd\n");
        exit(EXIT_FAILURE);
    }

    send_icmp_echo(argv[3], argv[4]);

    exit(EXIT_SUCCESS);
}
