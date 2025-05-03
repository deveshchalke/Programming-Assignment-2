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
    int i; /* C89-compliant declaration */
    for(i = 0; i < 20; i++) {
        checksum += (int)packet.payload[i];
    }
    return checksum;
}

bool IsCorrupted(struct pkt packet) {
    return packet.checksum != ComputeChecksum(packet);
}

/* -------------------- Sender (A) Code -------------------- */

void A_output(struct msg message) {
    if(next_seqnum < send_base + WINDOWSIZE) {
        struct pkt sendpkt;
        int window_pos = next_seqnum % WINDOWSIZE;
        int i; /* C89-compliant declaration */

        /* Create packet */
        sendpkt.seqnum = next_seqnum % SEQSPACE;
        sendpkt.acknum = NOTINUSE;
        for(i = 0; i < 20; i++) {
            sendpkt.payload[i] = message.data[i];
        }
        sendpkt.checksum = ComputeChecksum(sendpkt);

        /* Store in window */
        sender_window[window_pos] = sendpkt;
        ack_status[window_pos] = 0;

        /* Start timer for first packet in window */
        if(next_seqnum == send_base) {
            starttimer(A, RTT);
        }

        /* Transmit packet */
        if(TRACE > 0) printf("A: Sending packet %d\n", sendpkt.seqnum);
        tolayer3(A, sendpkt);

        next_seqnum++;
    } else {
        if(TRACE > 0) printf("A: Window full, message dropped\n");
        window_full++;
    }
}

void A_input(struct pkt packet) {
    if(!IsCorrupted(packet)) {
        int ack_num = packet.acknum;
        if(TRACE > 0) printf("A: Received valid ACK %d\n", ack_num);

        if(ack_num >= send_base && ack_num < next_seqnum) {
            int pos = ack_num % WINDOWSIZE;
            if(!ack_status[pos]) {
                ack_status[pos] = 1;
                new_ACKs++;
                
                /* Slide window forward */
                while(ack_status[send_base % WINDOWSIZE]) {
                    send_base++;
                }
                
                /* Manage timer */
                stoptimer(A);
                if(send_base < next_seqnum) {
                    starttimer(A, RTT);
                }
            }
        }
    } else if(TRACE > 0) {
        printf("A: Discarded corrupted ACK\n");
    }
}

void A_timerinterrupt(void) {
    if(TRACE > 0) printf("A: Timer expired, resending base packet %d\n", send_base);
    
    /* Resend oldest unACKed packet */
    tolayer3(A, sender_window[send_base % WINDOWSIZE]);
    packets_resent++;
    
    /* Restart timer */
    starttimer(A, RTT);
}

void A_init(void) {
    send_base = 0;
    next_seqnum = 0;
    memset(ack_status, 0, sizeof(ack_status));
    if(TRACE > 0) printf("A: SR sender initialized\n");
}

/* ------------------- Receiver (B) Code ------------------- */

void B_input(struct pkt packet) {
    struct pkt ackpkt;
    int seqnum = packet.seqnum;
    int i; /* C89-compliant declaration */

    /* Initialize ACK packet */
    ackpkt.acknum = seqnum;
    ackpkt.seqnum = NOTINUSE;
    for(i = 0; i < 20; i++) {
        ackpkt.payload[i] = '0';
    }
    ackpkt.checksum = ComputeChecksum(ackpkt);

    if(!IsCorrupted(packet)) {
        int window_start = rcv_base;
        int window_end = (rcv_base + WINDOWSIZE - 1) % SEQSPACE;
        int in_window = 0;
        int buffer_pos;

        /* Check if in receive window */
        if((window_start <= window_end && seqnum >= window_start && seqnum <= window_end) ||
           (window_start > window_end && (seqnum >= window_start || seqnum <= window_end))) {
            in_window = 1;
        }

        if(in_window) {
            buffer_pos = seqnum % WINDOWSIZE;
            if(!received[buffer_pos]) {
                receiver_buffer[buffer_pos] = packet;
                received[buffer_pos] = 1;
                packets_received++;
                if(TRACE > 1) printf("B: Buffered packet %d\n", seqnum);
            }
        }
    } else if(TRACE > 0) {
        printf("B: Discarded corrupted packet\n");
    }

    /* Always send ACK */
    tolayer3(B, ackpkt);
    if(TRACE > 1) printf("B: Sent ACK %d\n", ackpkt.acknum);

    /* Deliver in-order packets */
    while(received[rcv_base % WINDOWSIZE]) {
        tolayer5(B, receiver_buffer[rcv_base % WINDOWSIZE].payload);
        if(TRACE > 0) printf("B: Delivered packet %d\n", rcv_base);
        received[rcv_base % WINDOWSIZE] = 0;
        rcv_base = (rcv_base + 1) % SEQSPACE;
    }
}

void B_init(void) {
    rcv_base = 0;
    memset(received, 0, sizeof(received));
    if(TRACE > 0) printf("B: SR receiver initialized\n");
}

/* --------------- Unused Bidirectional Code --------------- */
void B_output(struct msg message) {}
void B_timerinterrupt(void) {}
