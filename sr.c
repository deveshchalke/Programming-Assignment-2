#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "emulator.h"
#include "sr.h"

#define RTT          16.0
#define WINDOWSIZE   6
#define SEQSPACE     7
#define NOTINUSE    (-1)

/* Sender (A) state */
static struct pkt buffer[WINDOWSIZE];
static int windowfirst, windowlast, windowcount;
static int A_nextseqnum;
static bool acked[WINDOWSIZE];

/* Receiver (B) state (stubs) */
static int expectedseqnum;
static int B_nextseqnum;
#define RCV_BUFFER_SIZE SEQSPACE
static struct pkt recv_buffer[RCV_BUFFER_SIZE];
static bool received[RCV_BUFFER_SIZE];

/* Utilities */
int ComputeChecksum(struct pkt packet) {
  int checksum = packet.seqnum + packet.acknum;
  int i;
  for (i = 0; i < 20; i++)
    checksum += (int)packet.payload[i];
  return checksum;
}
bool IsCorrupted(struct pkt packet) {
  return packet.checksum != ComputeChecksum(packet);
}

/* A_init */
void A_init(void) {
  windowfirst   = 0;
  windowlast    = -1;
  windowcount   = 0;
  A_nextseqnum  = 0;
  for (int i = 0; i < WINDOWSIZE; i++) acked[i] = false;
}

/* A_output: buffer & send if window not full */
void A_output(struct msg message) {
  struct pkt sendpkt;
  int i;

  if (windowcount < WINDOWSIZE) {
    sendpkt.seqnum = A_nextseqnum;
    sendpkt.acknum = NOTINUSE;
    for (i = 0; i < 20; i++) sendpkt.payload[i] = message.data[i];
    sendpkt.checksum = ComputeChecksum(sendpkt);

    windowlast = (windowfirst + windowcount) % WINDOWSIZE;
    buffer[windowlast] = sendpkt;
    acked[windowlast] = false;
    windowcount++;

    if (TRACE > 0)
      printf("Sending packet %d to layer 3\n", sendpkt.seqnum);
    tolayer3(A, sendpkt);

    if (windowcount == 1)
      starttimer(A, RTT);

    A_nextseqnum = (A_nextseqnum + 1) % SEQSPACE;
  } else {
    if (TRACE > 0) printf("A: Window full, dropping message\n");
    window_full++;
  }
}

/* Stubs for rest */
void A_input(struct pkt packet)   {}
void A_timerinterrupt(void)       {}
void B_init(void)                 {}
void B_input(struct pkt packet)   {}
void B_output(struct msg message) {}
void B_timerinterrupt(void)       {}
