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

#include "ip.h"
#include "arp.h"
#include "icmp.h"


// read-only after setup
static struct route_entry *route_head = NULL;

struct route_entry *route_lookup(struct in_addr addr, struct in_addr netmask) {
    struct route_entry *entry;
    for (entry = route_head; entry != NULL; entry = entry->next)
        if (addr.s_addr == entry->dest.s_addr &&
            netmask.s_addr == entry->netmask.s_addr)
            break;
    return entry;
}

struct route_entry *route_match(struct in_addr addr) {
    struct route_entry *entry;
    for (entry = route_head; entry != NULL; entry = entry->next)
        if ((addr.s_addr & entry->netmask.s_addr) == entry->dest.s_addr)
            break;
    return entry;
}

struct route_entry *route_alloc(struct in_addr dest, struct in_addr netmask,
                                struct in_addr gateway, struct interface_entry *interface) {
    struct route_entry *entry = NULL;

    entry = malloc(sizeof(struct route_entry));
    entry->dest = dest;
    entry->netmask = netmask;
    entry->gateway = gateway;
    entry->interface = interface;

    // add entry to route_list
    entry->next = route_head;
    route_head = entry;

    return entry;
}

struct route_entry *route_add(char *dest_str, char *netmask_str, char *gateway_str) {
    struct route_entry *entry = NULL;
    struct in_addr dest, netmask, gateway;

    if (inet_aton(dest_str, &dest) == 0) {
        fprintf(stderr, "route_add: invalid ip %s\n", dest_str);
        return entry;
    }
    if (inet_aton(netmask_str, &netmask) == 0) {
        fprintf(stderr, "route_add: invalid netmask %s\n", netmask_str);
        return entry;
    }

    if ((entry = route_lookup(dest, netmask)) != NULL) {
        fprintf(stderr, "route_add: %s %s already exists\n", dest_str, netmask_str);
        return entry;
    }

    if (inet_aton(gateway_str, &gateway) == 0) {
        fprintf(stderr, "route_add: invalid gateway %s\n", gateway_str);
        return entry;
    }

    // TODO match gateway in inet_list
    struct inet_entry *i_entry = inet_match(gateway);
    if (i_entry == NULL) {
        fprintf(stderr, "route_add: %s no such process\n", gateway_str);
        return entry;
    }

    entry = route_alloc(dest, netmask, gateway, i_entry->interface);
    return entry;
}

int ip_forward(int sockfd, void *buffer, size_t nbytes) {
    struct ip *iph = buffer;

    // TODO we can decrease ttl && recalculate checksum here
    // lookup route table for route_entry
    struct route_entry *r_entry = route_match(iph->ip_dst);
    if (r_entry == NULL) {
        fprintf(stderr, "ip_forward: no route for %s\n", inet_ntoa(iph->ip_dst));
        return -1;
    }
    // get arp_entry for gateway or daddr if we are in the same subnet with dest
    struct arp_entry *a_entry = NULL;
    if (r_entry->gateway.s_addr != 0)
        a_entry = arp_lookup(r_entry->gateway, 5);
    else
        a_entry = arp_lookup(iph->ip_dst, 5);

    if (a_entry == NULL) {
        fprintf(stderr, "ip_forward: no arp for");
        if (r_entry->gateway.s_addr != 0)
            fprintf(stderr, "%s\n", inet_ntoa(r_entry->gateway));
        else
            fprintf(stderr, "%s\n", inet_ntoa(iph->ip_dst));
        return -1;
    }

    // ip_forward packet to specific interface
    struct sockaddr_ll dest_addr = {
            .sll_family = AF_PACKET,
            .sll_protocol = htons(ETH_P_IP),
            .sll_halen = ETH_ALEN,
            .sll_ifindex = r_entry->interface->index,
    };
    memcpy(&dest_addr.sll_addr, &a_entry->mac_addr, ETH_ALEN);

    if (sendto(sockfd, buffer, nbytes, 0, (struct sockaddr *) &dest_addr, sizeof(dest_addr)) < 0) {
        fprintf(stderr, "ip_forward: sendto: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

void ip_build_hdr(struct ip *iph, struct in_addr src, struct in_addr dst,
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

