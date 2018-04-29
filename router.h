//
// Created by fuji on 18-4-29.
//

#ifndef STATIC_ROUTE_ROUTER_H
#define STATIC_ROUTE_ROUTER_H

#include <netinet/in.h>
#include <net/ethernet.h>
#include <net/if.h>

struct interface_entry {
    char name[IF_NAMESIZE];
    struct ether_addr addr;
};

struct route_entry {
    struct in_addr dest;
    struct in_addr netmask;
    struct in_addr gateway;
    struct interface_entry *interface;
};


#endif //STATIC_ROUTE_ROUTER_H
