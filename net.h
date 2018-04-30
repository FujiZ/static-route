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

struct inet_entry *inet_lookup(struct in_addr addr);

#endif //STATIC_ROUTE_NET_H
