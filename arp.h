//
// Created by fuji on 18-4-29.
//

#ifndef STATIC_ROUTE_ARP_H
#define STATIC_ROUTE_ARP_H

#include <netinet/in.h>
#include <net/ethernet.h>

#include <linux/if_arp.h>

struct arp_entry {
    struct in_addr ip_addr;
    struct ether_addr mac_addr;
    struct arp_entry *next;
};

struct arp_packet {
    struct arphdr ar_hdr;
    struct ether_addr ar_sha;    /* sender hardware address	*/
    struct in_addr ar_sip;        /* sender IP address		*/
    struct ether_addr ar_tha;    /* target hardware address	*/
    struct in_addr ar_tip;        /* target IP address		*/
};

struct arp_entry *arp_lookup(struct in_addr addr, int n);

#endif //STATIC_ROUTE_ARP_H
