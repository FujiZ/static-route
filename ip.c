//
// Created by fuji on 18-4-29.
//

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <net/ethernet.h>
#include <netinet/ip.h>

#include <linux/if_packet.h>

#include "arp.h"
#include "ip.h"

#define BUF_LEN 2048

// read-only after setup
static struct ip_route_entry *ip_route_head = NULL;

static struct ip_route_entry *ip_route_lookup(struct in_addr addr, struct in_addr netmask) {
    struct ip_route_entry *entry;
    for (entry = ip_route_head; entry != NULL; entry = entry->next)
        if (addr.s_addr == entry->dest.s_addr &&
            netmask.s_addr == entry->netmask.s_addr)
            break;
    return entry;
}

struct ip_route_entry *ip_route_match(struct in_addr addr) {
    struct ip_route_entry *entry;
    for (entry = ip_route_head; entry != NULL; entry = entry->next)
        if ((addr.s_addr & entry->netmask.s_addr) == entry->dest.s_addr)
            break;
    return entry;
}

static struct ip_route_entry *ip_route_alloc(struct in_addr dest, struct in_addr netmask,
                                             struct in_addr gateway, struct interface_entry *interface) {
    struct ip_route_entry *entry;

    entry = malloc(sizeof(*entry));
    entry->dest = dest;
    entry->netmask = netmask;
    entry->gateway = gateway;
    entry->interface = interface;

    // add entry to route_list
    entry->next = ip_route_head;
    ip_route_head = entry;

    return entry;
}

struct ip_route_entry *ip_route_add(char *dest_str, char *netmask_str, char *gateway_str) {
    struct in_addr dest, netmask, gateway;

    if (inet_aton(dest_str, &dest) == 0) {
        fprintf(stderr, "ip_route_add: invalid ip %s\n", dest_str);
        return NULL;
    }
    if (inet_aton(netmask_str, &netmask) == 0) {
        fprintf(stderr, "ip_route_add: invalid netmask %s\n", netmask_str);
        return NULL;
    }

    // make sure the dest is not a particular ip address
    dest.s_addr = dest.s_addr & netmask.s_addr;
    if (ip_route_lookup(dest, netmask) != NULL) {
        fprintf(stderr, "ip_route_add: %s %s already exists\n", dest_str, netmask_str);
        return NULL;
    }

    if (inet_aton(gateway_str, &gateway) == 0) {
        fprintf(stderr, "ip_route_add: invalid gateway %s\n", gateway_str);
        return NULL;
    }

    // match gateway in inet_list
    // if gateway == 0, we send to local interface
    struct inet_entry *in_entry;
    if (gateway.s_addr != 0)
        in_entry = inet_match(gateway);
    else
        in_entry = inet_match(dest);

    if (in_entry == NULL) {
        fprintf(stderr, "ip_route_add: %s no such process\n", gateway_str);
        return NULL;
    }

    return ip_route_alloc(dest, netmask, gateway, in_entry->interface);
}

// assume the len and checksum of this packet is right
int ip_send(int sockfd, void *buffer, size_t nbytes) {
    struct ip *iph = buffer;

    // lookup route table for ip_route_entry
    struct ip_route_entry *ir_entry = ip_route_match(iph->ip_dst);
    if (ir_entry == NULL) {
        fprintf(stderr, "ip_send: no route for %s\n", inet_ntoa(iph->ip_dst));
        return -1;
    }
    // get arp_entry for gateway or daddr if we are in the same subnet with dest
    struct arp_entry *ar_entry;
    if (ir_entry->gateway.s_addr != 0)
        ar_entry = arp_lookup(ir_entry->gateway, 5);
    else
        ar_entry = arp_lookup(iph->ip_dst, 5);

    if (ar_entry == NULL) {
        fprintf(stderr, "ip_send: no arp for ");
        if (ir_entry->gateway.s_addr != 0)
            fprintf(stderr, "%s\n", inet_ntoa(ir_entry->gateway));
        else
            fprintf(stderr, "%s\n", inet_ntoa(iph->ip_dst));
        return -1;
    }

    // send packet to specific interface
    struct sockaddr_ll dest_addr = {
            .sll_family = AF_PACKET,
            .sll_protocol = htons(ETH_P_IP),
            .sll_halen = ETH_ALEN,
            .sll_ifindex = ir_entry->interface->index,
    };
    memcpy(&dest_addr.sll_addr, &ar_entry->mac_addr, ETH_ALEN);

    if (sendto(sockfd, buffer, nbytes, 0, (struct sockaddr *) &dest_addr, sizeof(dest_addr)) < 0) {
        fprintf(stderr, "ip_send: sendto: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

/*
// assume the len and checksum of this packet is right
static int ip_forward(int sockfd, void *buffer, size_t nbytes) {
    struct ip *iph = buffer;
    return ip_send(sockfd, buffer, nbytes);
}
 */

void ip_build_header(struct ip *iph, struct in_addr src, struct in_addr dst,
                     u_int8_t protocol, unsigned int data_len) {
    static unsigned short count = 0;

    // call memset to clear iph
    memset(iph, 0, sizeof(struct ip));

    // fill in ip header
    iph->ip_v = IPVERSION;
    iph->ip_hl = sizeof(struct ip) / 4;
    iph->ip_len = htons(sizeof(struct ip) + data_len);
    iph->ip_id = htons(++count);
    iph->ip_ttl = IPDEFTTL;
    iph->ip_p = protocol;
    iph->ip_src = src;
    iph->ip_dst = dst;

    // calculate ip header checksum
    iph->ip_sum = inet_cksum((unsigned short *) iph, sizeof(struct ip), 0);
}

// route daemon
void *ip_routed(void *func) {
    unsigned char buffer[BUF_LEN];

    ip_recv_callback receive = func;

    int sockfd = socket(AF_PACKET, SOCK_DGRAM, htons(ETH_P_IP));
    if (sockfd < 0) {
        fprintf(stderr, "ip_routed: socket: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    ssize_t nbytes;
    struct sockaddr_ll addr;
    socklen_t addr_len = sizeof(addr);

    while ((nbytes = recvfrom(sockfd, buffer, BUF_LEN, 0,
                              (struct sockaddr *) &addr, &addr_len)) > 0) {
        // we should only care about incoming uni&broad-cast packet
        if (addr.sll_hatype != ARPHRD_ETHER ||
            addr.sll_pkttype == PACKET_LOOPBACK ||
            (addr.sll_pkttype != PACKET_HOST &&
             addr.sll_pkttype != PACKET_BROADCAST))
            continue;

        if (nbytes < sizeof(struct ip))
            continue;
        // check if we should receive this packet
        struct ip *iph = (struct ip *) buffer;
        // check ip header checksum
        unsigned short sum = inet_cksum((unsigned short *) iph, iph->ip_hl * 4, 0);
        if (sum != 0) {
            fprintf(stderr, "ip_routed: bad packet\n");
            continue;
        }
        // decrease ttl && recalculate checksum
        if (--iph->ip_ttl == 0) {
            fprintf(stderr, "ip_routed: ttl expired\n");
            continue;
        }
        iph->ip_sum = 0;
        iph->ip_sum = inet_cksum((unsigned short *) iph, sizeof(*iph), 0);

        if (inet_lookup(iph->ip_dst) == NULL)
            ip_send(sockfd, buffer, (size_t) nbytes);
        else
            receive(sockfd, buffer, (size_t) nbytes);
    }
    if (close(sockfd) < 0)
        fprintf(stderr, "ip_routed: close: %s\n", strerror(errno));

    return NULL;
}

