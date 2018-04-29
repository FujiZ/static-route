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
#include <unistd.h>

#include "router.h"
#include "arp.h"

#define BUF_LEN 2048

static struct route_entry *route_head = NULL;

void usage(void) {
    fprintf(stderr, "Usage: router interface route\n");
    exit(EXIT_FAILURE);
}

struct route_entry *route_lookup(in_addr_t addr) {
    struct route_entry *entry;
    for (entry = route_head; entry != NULL; entry = entry->next)
        if ((addr & entry->netmask.s_addr) == entry->dest.s_addr)
            break;
    return entry;
}

int ip_route(void *buffer, size_t nbytes) {
    struct ether_header *etherh = buffer;
    struct iphdr *iph = (struct iphdr *) &etherh[1];
    // TODO we do not recalculate checksum now
    // lookup route table for route_entry
    struct route_entry *entry = route_lookup(iph->daddr);
    // TODO get arp_entry for gateway
    struct arp_entry *arp_e = arp_lookup(entry->gateway);
    // change ether saddr & daddr
    memcpy(etherh->ether_shost, &entry->interface->addr, ETH_ALEN);
    memcpy(etherh->ether_dhost, &arp_e->mac_addr, ETH_ALEN);
    // TODO init an interface to send packet
    int sockfd;
    if ((sockfd = socket(AF_PACKET, SOCK_RAW, IPPROTO_RAW)) < 0) {
        fprintf(stderr, "ip_route: sendto: %s\n", strerror(errno));
        return -1;
    }
    if(setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, ));
    struct ifreq bvb;
    // TODO forward to interface
    struct sockaddr_ll dest_addr = {
            .sll_family = AF_PACKET,
            .sll_protocol = htons(ETH_P_IP),
            .sll_halen = ETH_ALEN,
            .sll_ifindex = entry->interface->index,
    };
    memcpy(&dest_addr.sll_addr, &arp_e->mac_addr, ETH_ALEN);
    if (sendto(sockfd, buffer, nbytes, 0, (struct sockaddr *) &dest_addr, sizeof(dest_addr)) < 0) {
        fprintf(stderr, "ip_route: sendto: %s\n", strerror(errno));
        return -1;
    }
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
    static unsigned char buffer[BUF_LEN];
    int sockfd;
    ssize_t nbytes;
    if ((sockfd = socket(AF_PACKET, SOCK_RAW, IPPROTO_RAW)) < 0) {
        fprintf(stderr, "router: socket: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    while ((nbytes = recv(sockfd, buffer, BUF_LEN, 0)) > 0) {
        if (nbytes < sizeof(struct ether_header) + sizeof(struct iphdr))
            continue;
        ip_route(buffer, (size_t) nbytes);
    }
}

int main() {
    // TODO init interface list
    // add default route when init interface
    // TODO init route table
    // TODO start arp daemon
    // TODO start router
    return 0;
}
