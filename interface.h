//
// Created by fuji on 18-5-1.
//

#ifndef STATIC_ROUTE_INTERFACE_H
#define STATIC_ROUTE_INTERFACE_H


#include <net/if.h>

struct interface_entry {
    char name[IFNAMSIZ];
    struct ether_addr addr;
    int index;
    struct interface_entry *next;
};

struct interface_entry *interface_lookup(char *name);

struct interface_entry *interface_alloc(char *name);

#endif //STATIC_ROUTE_INTERFACE_H
