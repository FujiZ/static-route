//
// Created by fuji on 18-4-30.
//


#include <stdlib.h>
#include <string.h>
#include <netinet/ip.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "inet.h"

// read-only after setup
struct inet_entry *inet_head = NULL;

// find the inet in the same subnet with addr
struct inet_entry *inet_match(struct in_addr addr) {
    struct inet_entry *entry;
    for (entry = inet_head; entry != NULL; entry = entry->next)
        if ((addr.s_addr & entry->netmask.s_addr) ==
            (entry->addr.s_addr & entry->netmask.s_addr))
            break;
    return entry;
}

struct inet_entry *inet_lookup(struct in_addr addr) {
    struct inet_entry *entry;
    for (entry = inet_head; entry != NULL; entry = entry->next)
        if (addr.s_addr == entry->addr.s_addr)
            break;
    return entry;
}

struct inet_entry *inet_alloc(struct in_addr addr, struct in_addr netmask,
                              struct interface_entry *interface) {
    struct inet_entry *entry = NULL;

    entry = malloc(sizeof(struct inet_entry));
    entry->addr = addr;
    entry->netmask = netmask;
    entry->interface = interface;

    // add entry to inet_list
    entry->next = inet_head;
    inet_head = entry;

    return 0;
}

struct inet_entry *inet_add(char *addr_str, char *netmask_str, char *interface_str) {
    struct inet_entry *entry = NULL;
    struct in_addr addr, netmask;
    struct interface_entry *interface;

    if (inet_aton(addr_str, &addr) == 0) {
        fprintf(stderr, "inet_add: invalid ip %s\n", addr_str);
        return entry;
    }

    // check if we already have this entry
    if ((entry = inet_lookup(addr)) != NULL) {
        fprintf(stderr, "inet_add: %s already exists\n", addr_str);
        return entry;
    }
    if (inet_aton(netmask_str, &netmask) == 0) {
        fprintf(stderr, "inet_add: invalid netmask %s\n", netmask_str);
        return entry;
    }

    interface = interface_lookup(interface_str);
    if (interface == NULL && (interface = interface_alloc(interface_str)) == NULL) {
        fprintf(stderr, "inet_add: invalid interface %s\n", interface_str);
        return entry;
    }

    entry = inet_alloc(addr, netmask, interface);
    return entry;
}

unsigned short
inet_cksum(const unsigned short *addr, int len, unsigned short csum) {
    int nleft = len;
    const unsigned short *w = addr;
    unsigned short answer;
    unsigned int sum = csum;

    /*
     *  Our algorithm is simple, using a 32 bit accumulator (sum),
     *  we add sequential 16 bit words to it, and at the end, fold
     *  back all the carry bits from the top 16 bits into the lower
     *  16 bits.
     */
    while (nleft > 1) {
        sum += *w++;
        nleft -= 2;
    }

    /* mop up an odd byte, if necessary */
    if (nleft == 1)
        sum += htons((unsigned short) (*(unsigned char *) w) << 8);

    /*
     * add back carry outs from top 16 bits to low 16 bits
     */
    sum = (sum >> 16) + (sum & 0xffff);    /* add hi 16 to low 16 */
    sum += (sum >> 16);                    /* add carry */
    answer = (unsigned short) ~sum;        /* truncate to 16 bits */
    return (answer);
}

