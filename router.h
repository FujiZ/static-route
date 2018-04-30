//
// Created by fuji on 18-4-29.
//

#ifndef STATIC_ROUTE_ROUTER_H
#define STATIC_ROUTE_ROUTER_H

#include <netinet/in.h>

#include "net.h"

struct route_entry {
    struct in_addr dest;
    struct in_addr netmask;
    struct in_addr gateway;
    struct interface_entry *interface;
    struct route_entry *next;
};

struct route_entry *route_lookup(struct in_addr addr);

int forward(void *buffer, size_t nbytes);

#endif //STATIC_ROUTE_ROUTER_H
