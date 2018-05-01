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
#include "icmp.h"

#define BUF_LEN 2048

// read-only after init
static struct route_entry *route_head = NULL;

struct route_entry *route_lookup(struct in_addr addr) {
    struct route_entry *entry;
    for (entry = route_head; entry != NULL; entry = entry->next)
        if ((addr.s_addr & entry->netmask.s_addr) == entry->dest.s_addr)
            break;
    return entry;
}

int forward(int sockfd, void *buffer, size_t nbytes) {
    struct ip *iph = buffer;

    // TODO we can decrease ttl && recalculate checksum here
    // lookup route table for route_entry
    struct route_entry *r_entry = route_lookup(iph->ip_dst);
    if (r_entry == NULL) {
        fprintf(stderr, "forward: no route for %s\n", inet_ntoa(iph->ip_dst));
        return -1;
    }
    // get arp_entry for gateway or daddr if we are in the same subnet with dest
    struct arp_entry *a_entry = NULL;
    if (r_entry->gateway.s_addr != 0)
        a_entry = arp_lookup(r_entry->gateway, 5);
    else
        a_entry = arp_lookup(iph->ip_dst, 5);

    if (a_entry == NULL) {
        fprintf(stderr, "forward: no arp for");
        if (r_entry->gateway.s_addr != 0)
            fprintf(stderr, "%s\n", inet_ntoa(r_entry->gateway));
        else
            fprintf(stderr, "%s\n", inet_ntoa(iph->ip_dst));
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

    if (sendto(sockfd, buffer, nbytes, 0, (struct sockaddr *) &dest_addr, sizeof(dest_addr)) < 0) {
        fprintf(stderr, "forward: sendto: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

int receive(int sockfd, void *buffer, size_t nbytes) {
    // only process icmp packet for now
    struct ip *iph = buffer;

    if (nbytes < ntohs(iph->ip_len))
        return -1;
    // TODO we can calculate ip checksum here

    switch (iph->ip_p) {
        case IPPROTO_ICMP:
            handle_icmp(sockfd, buffer, nbytes);
            break;
        default:
            break;
    }
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
            forward(sockfd, buffer, (size_t) nbytes);
        else
            receive(sockfd, buffer, (size_t) nbytes);
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
