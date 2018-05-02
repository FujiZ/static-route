//
// Created by fuji on 18-4-29.
//

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <net/ethernet.h>

#include <linux/if_packet.h>

#include "arp.h"
#include "inet.h"

#define BUF_LEN 128

static pthread_mutex_t arp_lock = PTHREAD_MUTEX_INITIALIZER;

static pthread_cond_t arp_cond = PTHREAD_COND_INITIALIZER;

static struct arp_entry *arp_head = NULL;

int send_arp_request(struct in_addr addr, struct inet_entry *i_entry) {
    static struct ether_addr broad_addr = {.ether_addr_octet={0xff, 0xff, 0xff, 0xff, 0xff, 0xff}};

    // create a socket to send packet
    int sockfd = socket(AF_PACKET, SOCK_DGRAM, htons(ETH_P_ARP));
    if (sockfd < 0) {
        fprintf(stderr, "send_arp_request: socket: %s\n", strerror(errno));
        return -1;
    }
    // build arp request
    struct arp_packet arpp = {
            .ar_hdr={
                    .ar_hrd=htons(ARPHRD_ETHER),
                    .ar_pro = htons(ETH_P_IP),
                    .ar_hln = ETH_ALEN,
                    .ar_pln = 4,
                    .ar_op = htons(ARPOP_REQUEST),
            },
            .ar_sip = i_entry->addr,
            .ar_sha = i_entry->interface->addr,
            .ar_tip = addr,
            .ar_tha = broad_addr,
    };

    // send arp request
    struct sockaddr_ll dest_addr = {
            .sll_family = AF_PACKET,
            .sll_protocol = htons(ETH_P_ARP),
            .sll_halen = ETH_ALEN,
            .sll_ifindex = i_entry->interface->index,
    };
    memcpy(&dest_addr.sll_addr, &broad_addr, ETH_ALEN);

    if (sendto(sockfd, &arpp, sizeof(arpp), 0, (struct sockaddr *) &dest_addr, sizeof(dest_addr)) < 0)
        fprintf(stderr, "send_arp_request: sendto: %s\n", strerror(errno));
    // fall to close socket
    if (close(sockfd) < 0) {
        fprintf(stderr, "send_arp_request: close: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

int send_arp_reply(int sockfd, struct arp_packet *request, struct inet_entry *i_entry) {
    // build arp request
    struct arp_packet arpp = {
            .ar_hdr={
                    .ar_hrd=htons(ARPHRD_ETHER),
                    .ar_pro = htons(ETH_P_IP),
                    .ar_hln = ETH_ALEN,
                    .ar_pln = 4,
                    .ar_op = htons(ARPOP_REPLY),
            },
            .ar_sip = i_entry->addr,
            .ar_sha = i_entry->interface->addr,
            .ar_tip = request->ar_sip,
            .ar_tha = request->ar_sha,
    };

    // send arp request
    struct sockaddr_ll dest_addr = {
            .sll_family = AF_PACKET,
            .sll_protocol = htons(ETH_P_ARP),
            .sll_halen = ETH_ALEN,
            .sll_ifindex = i_entry->interface->index,
    };
    memcpy(&dest_addr.sll_addr, &request->ar_sha, ETH_ALEN);

    if (sendto(sockfd, &arpp, sizeof(arpp), 0, (struct sockaddr *) &dest_addr, sizeof(dest_addr)) < 0) {
        fprintf(stderr, "send_arp_reply: sendto: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

struct arp_entry *__arp_lookup(struct in_addr addr) {
    struct arp_entry *entry;
    for (entry = arp_head; entry != NULL; entry = entry->next)
        if (addr.s_addr == entry->ip_addr.s_addr)
            break;
    return entry;
}

// may be called in another thread
struct arp_entry *arp_lookup(struct in_addr addr, int n) {
    struct arp_entry *entry = NULL;
    struct inet_entry *i_entry = NULL;

    if (pthread_mutex_lock(&arp_lock) != 0) {
        fprintf(stderr, "arp_lookup: pthread_mutex_lock: %s\n", strerror(errno));
        return entry;
    }

    for (int i = 0; i < n; ++i) {
        if ((entry = __arp_lookup(addr)) != NULL)
            break;

        if (i_entry == NULL && (i_entry = inet_match(addr)) == NULL) {
            fprintf(stderr, "arp_lookup: no sip for %s\n", inet_ntoa(addr));
            break;
        }

        // broadcast arp request
        send_arp_request(addr, i_entry);
        // wait for newly added arp_entry
        struct timespec timeout = {.tv_sec=time(NULL) + 1};
        pthread_cond_timedwait(&arp_cond, &arp_lock, &timeout);
        // the lock is re-acquired after return from cond_wait
    }
    pthread_mutex_unlock(&arp_lock);
    return entry;
}

// send reply if needed
int handle_arp_request(int sockfd, void *buffer, size_t len) {
    // we already checked data in arphdr
    if (len < sizeof(struct arp_packet))
        return -1;
    struct arp_packet *arpp = buffer;
    struct inet_entry *i_entry = inet_lookup(arpp->ar_tip);
    if (i_entry != NULL)
        // build arp reply and send it back
        return send_arp_reply(sockfd, arpp, i_entry);
    return 0;
}

int handle_arp_reply(void *buffer, size_t len) {
    // we already checked data in arphdr
    if (len < sizeof(struct arp_packet))
        return -1;

    struct arp_packet *arpp = buffer;

    if (pthread_mutex_lock(&arp_lock) != 0) {
        fprintf(stderr, "handle_arp_reply: pthread_mutex_lock: %s\n", strerror(errno));
        return -1;
    }

    // update ARP cache
    struct arp_entry *entry = __arp_lookup(arpp->ar_sip);
    if (entry == NULL) {
        entry = malloc(sizeof(struct arp_entry));
        entry->ip_addr = arpp->ar_sip;
        entry->next = arp_head;
        arp_head = entry;
    }
    entry->mac_addr = arpp->ar_sha;

    pthread_mutex_unlock(&arp_lock);
    // wakeup all sleeping thread
    pthread_cond_broadcast(&arp_cond);
    return 0;
}

// update arp table && send arp reply
void *arpd(void *arg) {
    unsigned char buffer[BUF_LEN];

    // create a socket to send && recv packet in arpd
    int sockfd = socket(AF_PACKET, SOCK_DGRAM, htons(ETH_P_ARP));
    if (sockfd < 0) {
        fprintf(stderr, "arpd: socket: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    ssize_t nbytes;
    struct sockaddr_ll addr;
    socklen_t addr_len = sizeof(addr);
    while ((nbytes = recvfrom(sockfd, buffer, BUF_LEN, 0,
                              (struct sockaddr *) &addr, &addr_len)) > 0) {
        // we should only care about incoming uni&broad-cast packet
        if (addr.sll_hatype != ARPHRD_ETHER ||
            addr.sll_pkttype == PACKET_LOOPBACK ||
            (addr.sll_pkttype != PACKET_HOST &&
             addr.sll_pkttype != PACKET_BROADCAST))
            continue;

        if (nbytes < sizeof(struct arphdr))
            continue;
        struct arphdr *arph = (struct arphdr *) buffer;
        // assertion for eth type && ip type
        if (ntohs(arph->ar_hrd) != ARPHRD_ETHER ||
            ntohs(arph->ar_pro) != ETH_P_IP ||
            arph->ar_hln != ETH_ALEN ||
            arph->ar_pln != 4)
            continue;

        switch (ntohs(arph->ar_op)) {
            case ARPOP_REQUEST:
                handle_arp_request(sockfd, buffer, (size_t) nbytes);
                break;
            case ARPOP_REPLY:
                handle_arp_reply(buffer, (size_t) nbytes);
                break;
            default:
                break;
        }
    }
    if (close(sockfd) < 0)
        fprintf(stderr, "arpd: close: %s\n", strerror(errno));

    return NULL;
}