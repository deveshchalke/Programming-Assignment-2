#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "emulator.h"
#include "sr.h"

/* ******************************************************************
   SR: basic send with full-window blocking
**********************************************************************/
#define RTT       16.0
#define WINDOWSIZE 6
#define SEQSPACE  12
#define NOTINUSE  (-1)

/* Sender state */
static struct pkt sender_window[WINDOWSIZE];
static int      ack_status[WINDOWSIZE];
static int      send_base;
static int      next_seqnum;

/* Receiver state (stubs) */
static int      rcv_base;
static struct pkt receiver_buffer[WINDOWSIZE];
static int      received[WINDOWSIZE];

/* Utilities */
int ComputeChecksum(struct pkt packet) {
    int c = packet.seqnum + packet.acknum;
    for(int i=0;i<20;i++) c += packet.payload[i];
    return c;
}
bool IsCorrupted(struct pkt packet) {
    return packet.checksum != ComputeChecksum(packet);
}

/* Sender init */
void A_init(void) {
    send_base   = 0;
    next_seqnum = 0;
    memset(ack_status, 0, sizeof ack_status);
}

/* Called by application */
void A_output(struct msg message) {
    if (next_seqnum < send_base + WINDOWSIZE) {
        struct pkt p;
        int pos = next_seqnum % WINDOWSIZE;
        p.seqnum   = next_seqnum % SEQSPACE;
        p.acknum   = NOTINUSE;
        memcpy(p.payload, message.data, 20);
        p.checksum = ComputeChecksum(p);

        sender_window[pos] = p;
        ack_status[pos]    = 0;

        if (TRACE>0) printf("Sending packet %d to layer 3\n", p.seqnum);
        tolayer3(A,p);

        if (next_seqnum == send_base)
            starttimer(A, RTT);

        next_seqnum++;
    } else {
        if (TRACE>0) printf("A: Window full, message dropped\n");
        window_full++;
    }
}


void A_input(struct pkt packet)   {}
void A_timerinterrupt(void)       {}
void B_init(void)                 {}
void B_input(struct pkt packet)   {}
void B_output(struct msg message) {}
void B_timerinterrupt(void)       {}
