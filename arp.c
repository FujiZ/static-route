//
// Created by fuji on 18-4-29.
//


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <linux/if_packet.h>

#include "arp.h"
#include "net.h"

#define BUF_LEN 1024

static pthread_mutex_t arp_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t arp_cond = PTHREAD_COND_INITIALIZER;
static struct arp_entry *arp_head = NULL;

int send_arp_request(struct in_addr addr) {
    unsigned char buffer[BUF_LEN];
    static struct ether_addr broad_addr = {.ether_addr_octet={0xff, 0xff, 0xff, 0xff, 0xff, 0xff}};

    // get the right src ip we want to fill in arp packet
    struct inet_entry *i_entry = inet_lookup(addr);
    if (i_entry == NULL) {
        fprintf(stderr, "send_arp_request: no ip for %s\n", inet_ntoa(addr));
        return -1;
    }

    // create a socket to send packet
    int sockfd = socket(AF_PACKET, SOCK_DGRAM, htons(ETH_P_ARP));
    if (sockfd < 0) {
        fprintf(stderr, "send_arp_request: socket: %s\n", strerror(errno));
        return -1;
    }
    // build arp request
    struct arp_header *arph = (struct arp_header *) buffer;
    arph->ar_hdr.ar_hrd = htons(ARPHRD_ETHER);
    arph->ar_hdr.ar_pro = htons(ETH_P_IP);
    arph->ar_hdr.ar_hln = ETH_ALEN;
    arph->ar_hdr.ar_pln = 4;
    arph->ar_hdr.ar_op = htons(ARPOP_REQUEST);
    arph->ar_sip = i_entry->addr;
    arph->ar_sha = i_entry->interface->addr;
    arph->ar_tip = addr;
    arph->ar_tha = broad_addr;

    // send arp request
    struct sockaddr_ll dest_addr = {
            .sll_family = AF_PACKET,
            .sll_protocol = htons(ETH_P_ARP),
            .sll_halen = ETH_ALEN,
            .sll_ifindex = i_entry->interface->index,
    };
    memcpy(&dest_addr.sll_addr, &broad_addr, ETH_ALEN);

    if (sendto(sockfd, buffer, sizeof(*arph), 0, (struct sockaddr *) &dest_addr, sizeof(dest_addr)) < 0)
        fprintf(stderr, "send_arp_request: sendto: %s\n", strerror(errno));
    // fall to close socket
    if (close(sockfd) < 0) {
        fprintf(stderr, "send_arp_request: close: %s\n", strerror(errno));
        return -1;
    }
}

struct arp_entry *arp_lookup(struct in_addr addr, int n) {
    struct arp_entry *entry = NULL;

    if (pthread_mutex_lock(&arp_lock) != 0) {
        fprintf(stderr, "arp_lookup: pthread_mutex_lock: %s\n", strerror(errno));
        return entry;
    }

    for (int i = 0; i < n; ++i) {
        for (entry = arp_head; entry != NULL; entry = entry->next)
            if (addr.s_addr == entry->ip_addr.s_addr)
                break;
        if (entry != NULL)
            break;

        // broadcast arp request
        send_arp_request(addr);
        // wait for newly added arp_entry
        struct timespec timeout = {.tv_sec=time(NULL) + 1};
        pthread_cond_timedwait(&arp_cond, &arp_lock, &timeout);
        // the lock is re-acquired after return from cond_wait
    }
    pthread_mutex_unlock(&arp_lock);
    return entry;
}


void process

void arpd(void) {
    unsigned char buffer[BUF_LEN];

    // TODO update arp table && send arp reply
    // create a socket to send packet
    int sockfd = socket(AF_PACKET, SOCK_DGRAM, htons(ETH_P_ARP));
    if (sockfd < 0) {
        fprintf(stderr, "arpd: socket: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    ssize_t nbytes;
    while ((nbytes = recv(sockfd, buffer, BUF_LEN, 0)) > 0) {
        if (nbytes < sizeof(struct arph))
            continue;
        struct arphdr *arph = (struct arphdr *) buffer;
        // assert for eth type && ip type
        if (ntohs(arph->ar_hrd) != ARPHRD_ETHER ||
            ntohs(arph->ar_pro) != ETH_P_IP ||
            arph->ar_hln != ETH_ALEN ||
            arph->ar_pln != 4)
            continue;
        switch (ntohs(arph->ar_op)){
            case ARPOP_REQUEST:
                //
                break;
            case ARPOP_REPLY:
                break;
            default:
                break;
        }
    }
    if (close(sockfd) < 0)
        fprintf(stderr, "routed: close: %s\n", strerror(errno));
}