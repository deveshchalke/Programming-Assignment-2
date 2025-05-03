#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "emulator.h"
#include "sr.h"

/* ******************************************************************
   Selective Repeat Protocol with Single Base Timer
**********************************************************************/
#define RTT 16.0
#define WINDOWSIZE 6
#define SEQSPACE 12
#define NOTINUSE (-1)

/********* Sender (A) Variables ************/
static struct pkt sender_window[WINDOWSIZE];
static int ack_status[WINDOWSIZE];
static int send_base;
static int next_seqnum;

/********* Receiver (B) Variables ************/
static int rcv_base;
static struct pkt receiver_buffer[WINDOWSIZE];
static int received[WINDOWSIZE];

/* ------------------- Utility Functions ------------------- */

int ComputeChecksum(struct pkt packet) {
    int checksum = packet.seqnum + packet.acknum;
    int i;
    for (i = 0; i < 20; i++) {
        checksum += (int)packet.payload[i];
    }
    return checksum;
}

bool IsCorrupted(struct pkt packet) {
    return packet.checksum != ComputeChecksum(packet);
}

/* -------------------- Sender (A) Code -------------------- */

void A_output(struct msg message) {
    if (next_seqnum < send_base + WINDOWSIZE) {
        struct pkt sendpkt;
        int window_pos = next_seqnum % WINDOWSIZE;
        int i;

        sendpkt.seqnum = next_seqnum % SEQSPACE;
        sendpkt.acknum = NOTINUSE;
        for (i = 0; i < 20; i++) {
            sendpkt.payload[i] = message.data[i];
        }
        sendpkt.checksum = ComputeChecksum(sendpkt);

        sender_window[window_pos] = sendpkt;
        ack_status[window_pos] = 0;

        /* Start timer only for first packet in window */
        if (next_seqnum == send_base) {
            starttimer(A, RTT);
        }

        if (TRACE > 0) printf("Sending packet %d to layer 3\n", sendpkt.seqnum);
        tolayer3(A, sendpkt);

        next_seqnum++;
    } else {
        if (TRACE > 0) printf("A: Window full, message dropped\n");
        window_full++;
    }
}

void A_input(struct pkt packet) {
    if (!IsCorrupted(packet)) {
        int ack_num = packet.acknum;
        if (TRACE > 0) printf("----A: uncorrupted ACK %d is received\n", ack_num);
        total_ACKs_received++;

        if (ack_num >= send_base && ack_num < send_base + WINDOWSIZE) {
            int pos = ack_num % WINDOWSIZE;
            if (!ack_status[pos]) {
                ack_status[pos] = 1;
                new_ACKs++;

                /* Slide window as far as possible */
                while (ack_status[send_base % WINDOWSIZE] && send_base < next_seqnum) {
                    ack_status[send_base % WINDOWSIZE] = 0;
                    send_base++;
                }

                /* Manage timer */
                stoptimer(A);
                if (send_base < next_seqnum) {
                    starttimer(A, RTT);
                }
            }
        }
    } else if (TRACE > 0) {
        printf("----A: corrupted ACK is received, do nothing!\n");
    }
}

void A_timerinterrupt(void) {
    int i;
    if (TRACE > 0) printf("----A: time out, resending base packet %d\n", send_base);
    
    /* Resend only the oldest unacked packet */
    if (send_base < next_seqnum) {
        int pos = send_base % WINDOWSIZE;
        if (TRACE > 0) printf("Resending packet %d\n", sender_window[pos].seqnum);
        tolayer3(A, sender_window[pos]);
        packets_resent++;
    }
    
    /* Restart timer */
    starttimer(A, RTT);
}

void A_init(void) {
    send_base = 0;
    next_seqnum = 0;
    memset(ack_status, 0, sizeof(ack_status));
}

/* ------------------- Receiver (B) Code ------------------- */

void B_input(struct pkt packet) {
    struct pkt ackpkt;
    int seqnum = packet.seqnum;
    int i;

    ackpkt.acknum = seqnum;
    ackpkt.seqnum = NOTINUSE;
    for (i = 0; i < 20; i++) {
        ackpkt.payload[i] = '0';
    }
    ackpkt.checksum = ComputeChecksum(ackpkt);

    if (!IsCorrupted(packet)) {
        int window_start = rcv_base;
        int window_end = (rcv_base + WINDOWSIZE - 1) % SEQSPACE;
        int in_window = 0;

        /* Check if in receive window */
        if ((window_start <= window_end && seqnum >= window_start && seqnum <= window_end) ||
            (window_start > window_end && (seqnum >= window_start || seqnum <= window_end))) {
            in_window = 1;
        }

        if (in_window) {
            int buffer_pos = seqnum % WINDOWSIZE;
            if (!received[buffer_pos]) {
                receiver_buffer[buffer_pos] = packet;
                received[buffer_pos] = 1;
                packets_received++;
                if (TRACE > 0) printf("----B: packet %d is correctly received, send ACK!\n", seqnum);
            }
        }

        /* Send ACK even for out-of-order but non-corrupted packets */
        tolayer3(B, ackpkt);
    } else if (TRACE > 0) {
        printf("----B: packet corrupted, discarding\n");
    }

    /* Deliver in-order packets */
    while (received[rcv_base % WINDOWSIZE]) {
        tolayer5(B, receiver_buffer[rcv_base % WINDOWSIZE].payload);
        received[rcv_base % WINDOWSIZE] = 0;
        rcv_base = (rcv_base + 1) % SEQSPACE;
    }
}

void B_init(void) {
    rcv_base = 0;
    memset(received, 0, sizeof(received));
}

/* --------------- Unused Bidirectional Code --------------- */
void B_output(struct msg message) {}
void B_timerinterrupt(void) {}
