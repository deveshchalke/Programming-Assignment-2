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

/* A_output unchanged */

/* A_input: mark ACK, handle duplicates, slide in‐order */
void A_input(struct pkt packet) {
  int i, index;
  if (IsCorrupted(packet)) {
    if (TRACE > 0) printf("A: corrupted ACK, ignoring\n");
    return;
  }

  if (TRACE > 0)
    printf("----A: uncorrupted ACK %d is received\n", packet.acknum);
  total_ACKs_received++;

  if (windowcount == 0) return;

  /* Find the buffer slot matching this ACK */
  index = windowfirst;
  for (i = 0; i < windowcount; i++) {
    if (buffer[index].seqnum == packet.acknum) {
      if (!acked[index]) {
        if (TRACE > 0) printf("----A: ACK %d is not a duplicate\n", packet.acknum);
        new_ACKs++;
        acked[index] = true;
      } else {
        if (TRACE > 0) printf("----A: duplicate ACK %d, no action\n", packet.acknum);
      }
      break;
    }
    index = (index + 1) % WINDOWSIZE;
  }

  /* Slide window over all in-order ACKed */
  while (windowcount > 0 && acked[windowfirst]) {
    acked[windowfirst] = false;
    windowfirst = (windowfirst + 1) % WINDOWSIZE;
    windowcount--;
  }

  /* Restart timer if needed */
  stoptimer(A);
  if (windowcount > 0)
    starttimer(A, RTT);
}

/* A_timerinterrupt stub */
void A_timerinterrupt(void) {}

/* B_init & B_input stubs */
void B_init(void)                 {}
void B_input(struct pkt packet)   {}
void B_output(struct msg message) {}
void B_timerinterrupt(void)       {}
