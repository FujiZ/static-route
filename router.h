//
// Created by fuji on 18-4-29.
//

#ifndef STATIC_ROUTE_ROUTER_H
#define STATIC_ROUTE_ROUTER_H

#include <netinet/in.h>
#include <net/ethernet.h>
#include <net/if.h>

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

struct route_entry {
    struct in_addr dest;
    struct in_addr netmask;
    struct in_addr gateway;
    struct interface_entry *interface;
    struct route_entry *next;
};

#endif //STATIC_ROUTE_ROUTER_H
