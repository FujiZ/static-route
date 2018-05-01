//
// Created by fuji on 18-4-29.
//

#ifndef STATIC_ROUTE_IP_H
#define STATIC_ROUTE_IP_H

#include <netinet/in.h>
#include <netinet/ip.h>

#include "inet.h"

struct ip_route_entry {
    struct in_addr dest;
    struct in_addr netmask;
    struct in_addr gateway;
    struct interface_entry *interface;
    struct ip_route_entry *next;
};

struct ip_route_entry *ip_route_add(char *dest_str, char *netmask_str, char *gateway_str);

int ip_send(int sockfd, void *buffer, size_t nbytes);

int ip_forward(int sockfd, void *buffer, size_t nbytes);

void ip_build_header(struct ip *iph, struct in_addr src, struct in_addr dst,
                     u_int8_t protocol, unsigned int data_len);

#endif //STATIC_ROUTE_IP_H
