//
// Created by fuji on 18-4-29.
//


#include <net/ethernet.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <linux/if_packet.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "router.h"
#include "arp.h"

#define BUF_LEN 2048

static struct route_entry *route_head = NULL;

void usage(void) {
    fprintf(stderr, "Usage: router interface route\n");
    exit(EXIT_FAILURE);
}

struct route_entry *route_lookup(struct in_addr addr) {
    struct route_entry *entry;
    for (entry = route_head; entry != NULL; entry = entry->next)
        if ((addr.s_addr & entry->netmask.s_addr) == entry->dest.s_addr)
            break;
    return entry;
}

int ip_route(void *buffer, size_t nbytes) {
    struct iphdr *iph = buffer;
    struct in_addr src_addr = {.s_addr = iph->daddr};
    // TODO we do not recalculate checksum now
    // lookup route table for route_entry
    struct route_entry *r_entry = route_lookup(src_addr);
    if (r_entry == NULL) {
        fprintf(stderr, "ip_route: no route for %s\n", inet_ntoa(src_addr));
        return -1;
    }
    // get arp_entry for gateway
    struct arp_entry *a_entry = arp_lookup(r_entry->gateway, 5);
    if (a_entry == NULL) {
        fprintf(stderr, "ip_route: no arp for %s\n", inet_ntoa(r_entry->gateway));
        return -1;
    }
    // create a socket to send packet
    int sockfd = socket(AF_PACKET, SOCK_DGRAM, htons(ETH_P_IP));
    if (sockfd < 0) {
        fprintf(stderr, "ip_route: socket: %s\n", strerror(errno));
        return -1;
    }
    // forward packet to specific interface
    struct sockaddr_ll dest_addr = {
            .sll_family = AF_PACKET,
            .sll_protocol = htons(ETH_P_IP),
            .sll_halen = ETH_ALEN,
            .sll_ifindex = r_entry->interface->index,
    };
    memcpy(&dest_addr.sll_addr, &a_entry->mac_addr, ETH_ALEN);

    if (sendto(sockfd, buffer, nbytes, 0, (struct sockaddr *) &dest_addr, sizeof(dest_addr)) < 0)
        fprintf(stderr, "ip_route: sendto: %s\n", strerror(errno));
    // fall to close socket

    if (close(sockfd) < 0) {
        fprintf(stderr, "ip_route: close: %s\n", strerror(errno));
        return -1;
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
        if (nbytes < sizeof(struct iphdr))
            continue;
        // TODO check if we should do local_deliver
        // else: we should route this packet
        ip_route(buffer, (size_t) nbytes);
    }
    if (close(sockfd) < 0)
        fprintf(stderr, "routed: close: %s\n", strerror(errno));
}

int main() {
    // TODO init interface list
    // add default route when init interface
    // TODO init route table
    // TODO start arp daemon
    // TODO start router daemon
    routed();
    return 0;
}
