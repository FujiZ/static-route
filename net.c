//
// Created by fuji on 18-4-30.
//


#include <stdlib.h>

#include "net.h"

struct inet_entry *inet_head = NULL;

struct inet_entry *inet_lookup(struct in_addr addr) {
    struct inet_entry *entry;
    for (entry = inet_head; entry != NULL; entry = entry->next)
        if ((addr.s_addr & entry->netmask.s_addr) ==
            (entry->addr.s_addr & entry->netmask.s_addr))
            break;
    return entry;
}
