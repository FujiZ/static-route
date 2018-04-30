//
// Created by fuji on 18-4-30.
//

#ifndef STATIC_ROUTE_ICMP_H
#define STATIC_ROUTE_ICMP_H

#include <stdlib.h>

int process_icmp(int sockfd, void *buffer, size_t nbytes);

#endif //STATIC_ROUTE_ICMP_H
