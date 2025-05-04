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

/* Receiver (B) stubs */
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
  windowfirst  = 0;
  windowlast   = -1;
  windowcount  = 0;
  A_nextseqnum = 0;
  for (int i = 0; i < WINDOWSIZE; i++) acked[i] = false;
}

/* A_output same as before */

/* A_input same as before */

/* A_timerinterrupt: resend only the first unACKed */
void A_timerinterrupt(void) {
  if (TRACE > 0) printf("----A: timeout, resending oldest unACKed\n");

  if (windowcount > 0) {
    int pos = windowfirst % WINDOWSIZE;
    if (TRACE > 0)
      printf("---A: resending packet %d\n", buffer[pos].seqnum);
    tolayer3(A, buffer[pos]);
    packets_resent++;
    starttimer(A, RTT);
  } else {
    if (TRACE > 0)
      printf("----A: timeout but no unACKed packets\n");
  }
}

/* B_init & B_input stubs */
void B_init(void)                 {}
void B_input(struct pkt packet)   {}
void B_output(struct msg message) {}
void B_timerinterrupt(void)       {}
