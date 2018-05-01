//
// Created by fuji on 18-5-1.
//



#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "arp.h"
#include "icmp.h"
#include "inet.h"
#include "ip.h"

#define BUF_LEN 2048

void usage(void) {
    fprintf(stderr, "Usage: router inet route\n");
    exit(EXIT_FAILURE);
}

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

// route daemon
void *routed(void *arg) {
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

    return NULL;
}

int main(int argc, char *argv[]) {
    FILE *fp;
    int n;

    char if_str[IFNAMSIZ];
    char addr_str[16];
    char netmask_str[16];
    char gateway_str[16];

    if (argc < 3)
        usage();

    // setup inet interface
    if ((fp = fopen(argv[1], "r")) == NULL) {
        fprintf(stderr, "router: fopen: %s\n", strerror(errno));
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
        fprintf(stderr, "router: interface config error\n");
        exit(EXIT_FAILURE);
    }
    fclose(fp);

    // setup route table
    if ((fp = fopen(argv[2], "r")) == NULL) {
        fprintf(stderr, "router: fopen: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    while ((n = fscanf(fp, "%15s %15s %15s", addr_str, netmask_str, gateway_str)) == 3) {
        // add default route for this entry
        if (ip_route_add(addr_str, netmask_str, gateway_str) == NULL)
            exit(EXIT_FAILURE);
    }
    if (n != EOF) {
        fprintf(stderr, "router: route config error\n");
        exit(EXIT_FAILURE);
    }
    fclose(fp);

    pthread_t arpd_tid;
    pthread_t routed_tid;
    // start arp daemon
    if (pthread_create(&arpd_tid, NULL, arpd, NULL) != 0) {
        fprintf(stderr, "router: can't start arpd\n");
        exit(EXIT_FAILURE);
    }
    // start router daemon
    if (pthread_create(&routed_tid, NULL, routed, NULL) != 0) {
        fprintf(stderr, "router: can't start routed\n");
        exit(EXIT_FAILURE);
    }

    pthread_join(routed_tid, NULL);

    exit(EXIT_SUCCESS);
}
