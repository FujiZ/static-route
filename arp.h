//
// Created by fuji on 18-4-29.
//

#ifndef STATIC_ROUTE_ARP_H
#define STATIC_ROUTE_ARP_H

#include <netinet/in.h>
#include <net/ethernet.h>

struct arp_entry{
    struct in_addr ip_addr;
    struct ether_addr mac_addr;
};

#endif //STATIC_ROUTE_ARP_H
