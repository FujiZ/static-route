//
// Created by fuji on 18-4-30.
//

#ifndef STATIC_ROUTE_NET_H
#define STATIC_ROUTE_NET_H

#include <net/ethernet.h>
#include <net/if.h>
#include <netinet/in.h>

struct interface_entry {
    char name[IFNAMSIZ];
    struct ether_addr addr;
    int index;
    struct interface_entry *next;
};

struct inet_entry {
    // we can bind multiple addr to one interface
    struct in_addr addr;
    struct in_addr netmask;
    struct interface_entry *interface;
    struct inet_entry *next;
};

struct inet_entry *inet_match(struct in_addr addr);

struct inet_entry *inet_lookup(struct in_addr addr);

unsigned short inet_cksum(const unsigned short *addr, int len, unsigned short csum);

void build_iphdr(struct ip *iph, struct in_addr src, struct in_addr dst,
                 u_int8_t protocol, unsigned int data_len);

#endif //STATIC_ROUTE_NET_H
