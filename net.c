//
// Created by fuji on 18-4-30.
//


#include <stdlib.h>

#include "net.h"

struct inet_entry *inet_head = NULL;

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
