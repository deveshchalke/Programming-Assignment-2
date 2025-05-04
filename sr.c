#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "emulator.h"
#include "sr.h"

#define RTT          16.0
#define WINDOWSIZE   6
#define SEQSPACE     7
#define NOTINUSE    (-1)

/* Sender (A) as before */
static struct pkt buffer[WINDOWSIZE];
static int windowfirst, windowlast, windowcount;
static int A_nextseqnum;
static bool acked[WINDOWSIZE];

/* Receiver (B) state */
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

/* A_init/A_output/A_input/A_timerinterrupt unchanged */

/* B_init */
void B_init(void) {
  expectedseqnum = 0;
  B_nextseqnum   = 1;
  for (int i = 0; i < RCV_BUFFER_SIZE; i++) received[i] = false;
}

/* B_input: buffer in‐window, always ACK, deliver in‐order */
void B_input(struct pkt packet) {
  struct pkt ackpkt;
  int i, s = packet.seqnum;

  /* Build ACK */
  ackpkt.acknum   = (!IsCorrupted(packet)) ? s
                    : (expectedseqnum + SEQSPACE - 1) % SEQSPACE;
  ackpkt.seqnum   = B_nextseqnum;
  for (i = 0; i < 20; i++) ackpkt.payload[i] = '0';
  ackpkt.checksum = ComputeChecksum(ackpkt);

  if (!IsCorrupted(packet)) {
    int start = expectedseqnum;
    int end   = (expectedseqnum + WINDOWSIZE - 1) % SEQSPACE;
    bool inWin = (start <= end)
                 ? (s >= start && s <= end)
                 : (s >= start || s <= end);
    if (inWin && !received[s]) {
      recv_buffer[s] = packet;
      received[s]    = true;
      packets_received++;
      if (TRACE > 0)
        printf("----B: buffered packet %d\n", s);
    }
  } else if (TRACE > 0) {
    printf("----B: corrupted packet, dropping\n");
  }

  /* Send ACK */
  tolayer3(B, ackpkt);
  if (TRACE > 0)
    printf("----B: sent ACK %d\n", ackpkt.acknum);

  /* Deliver all in‐order */
  while (received[expectedseqnum]) {
    tolayer5(B, recv_buffer[expectedseqnum].payload);
    if (TRACE > 0)
      printf("----B: delivered packet %d\n", expectedseqnum);
    received[expectedseqnum] = false;
    expectedseqnum = (expectedseqnum + 1) % SEQSPACE;
  }
}

void B_output(struct msg message) {}
void B_timerinterrupt(void)   {}
