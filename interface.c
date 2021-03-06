//
// Created by fuji on 18-5-1.
//

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <netinet/in.h>
#include <net/ethernet.h>
#include <sys/ioctl.h>

#include "interface.h"

// read-only after setup
struct interface_entry *interface_head = NULL;

struct interface_entry *interface_lookup(char *name) {
    struct interface_entry *entry;
    for (entry = interface_head; entry != NULL; entry = entry->next)
        if (strcmp(name, entry->name) == 0)
            break;
    return entry;
}

struct interface_entry *interface_alloc(char *name) {
    struct interface_entry *entry = NULL;
    int sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));

    if (sockfd < 0) {
        fprintf(stderr, "interface_alloc: socket: %s\n", strerror(errno));
        return entry;
    }
    struct ifreq req;
    memset(&req, 0, sizeof(struct ifreq));
    strncpy(req.ifr_name, name, IFNAMSIZ - 1);

    // get interface index
    if (ioctl(sockfd, SIOCGIFINDEX, &req) < 0) {
        fprintf(stderr, "interface_alloc: ioctl: %s\n", strerror(errno));
        return entry;
    }
    int if_index = req.ifr_ifindex;

    // get ethernet addr
    if (ioctl(sockfd, SIOCGIFHWADDR, &req) < 0) {
        fprintf(stderr, "interface_alloc: ioctl: %s\n", strerror(errno));
        return entry;
    }

    if (close(sockfd) < 0) {
        fprintf(stderr, "interface_alloc: close: %s\n", strerror(errno));
        return entry;
    }

    entry = malloc(sizeof(*entry));
    strncpy(entry->name, name, IFNAMSIZ - 1);
    memcpy(&entry->addr, req.ifr_hwaddr.sa_data, ETH_ALEN);
    entry->index = if_index;

    // add entry to interface_list
    entry->next = interface_head;
    interface_head = entry;

    return entry;
}

