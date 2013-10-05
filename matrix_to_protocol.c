// Copyright 2013 David Monro davidm@davidmonro.net

// Code in this file is responsible for taking the debounced matrix and
// generating protocol events.

// So for example, for a USB keyboard this should know all about how to
// generate the right report structures. In theory it shouldn't know
// much about the low level driving of the usb interface; that should be
// handled in usb_interface.c. It does however implement the ring buffer
// in case the USB transmitter is busy.

#include "kbdcont.h"
#include "keymap.h"

#define REPORTLENGTH 8
static unsigned char oldreport[REPORTLENGTH];
static unsigned char newreport[REPORTLENGTH];

static uint8_t currkey = 0;
static uint8_t rollover = 0;
static uint8_t have_a_live_key = 0;

// Ringbuffer stuff
#define RINGBUFFERSLOTS ((uint8_t)(8))
static uint8_t reportRingBuffer[RINGBUFFERSLOTS * REPORTLENGTH];
static uint8_t rbHeadIndx, rbTailIndx;


void queueReport(void)
{
    // Only bother enqueueing it if it is different to the last one
  if (memcmp((void *)oldreport, (void *)newreport, REPORTLENGTH) != 0) {
    if (((rbTailIndx + 1) % RINGBUFFERSLOTS) != rbHeadIndx) {
      memcpy(reportRingBuffer + REPORTLENGTH * rbTailIndx,
	(void *)newreport, REPORTLENGTH);
      SerialDebug(10, "enq %d\r\n", rbTailIndx);
      rbTailIndx++;
      rbTailIndx %= RINGBUFFERSLOTS;
    } else {
      SerialDebug(1, "QFULL h %d t %d\r\n", (int)rbHeadIndx,
	(int)rbTailIndx);
    }
    // even if we didn't queue it, no point getting into a loop trying to report it
    memcpy(oldreport, (void *)newreport, REPORTLENGTH);
  }
}

void *dequeueReport(void)
{
  void* ret;
  SerialDebug(10, "dq %d", rbHeadIndx);

  if (rbHeadIndx == rbTailIndx) {
    SerialDebug(10, " queue was empty\r\n");
    return NULL;
  }
  // We know there's something in the queue
  ret = reportRingBuffer + REPORTLENGTH * rbHeadIndx;
  SerialDebug(2, "dq %d ok\r\n", rbHeadIndx);
  rbHeadIndx++;
  rbHeadIndx %= RINGBUFFERSLOTS;
  return ret;
}

void initScanProcessor(void)
{
  memset(oldreport, 0, 8);
  memset(newreport, 0, 8);

  memset(reportRingBuffer, 0, RINGBUFFERSLOTS * 8);
  rbHeadIndx = 0;
  rbTailIndx = 0;
}

void beginScanEvent(void)
{
  // A scan is beginning.
  memset(newreport, 0, 8);
  currkey = 0;
  have_a_live_key = 0;
  rollover = 0;
}

void addScanPoint(kbdscanval_t scanpoint)
{
  // The matrix scanner found a live (debounced) key
  uint8_t key = pgm_read_byte(&(keymap[scanpoint]));

  SerialDebug(2, "SP %02x KEY %02x\r\n", scanpoint, key);

  if (key) {
    have_a_live_key = 1;
    if (key < 0xe0) {
      // Not a modifier
      newreport[2 + currkey] = key;
      currkey++;
      if (currkey > 5) {
	currkey = 5;
	rollover = 1;
      }
    } else {
      // Modifiers are 0xE0 to 0xE7 and map 1:1 to bits 0:7 of the first
      // byte of the report
      newreport[0] |= (1 << (key & 0x0f));
    }
  }
}

void invalidateScan(void)
{
  // The matrix scanner found an issue, probably ghosted keys
  rollover = 1;
}

void endScanEvent(void)
{

  static int8_t tries_before_sleep;

  if (rollover) {
    SerialDebug(1, "blocked keys\r\n");
    memset(newreport + 2, 0x01, 6);
  }

  if (pending_key_wake == 0) {
    tries_before_sleep = -1;
  } else {
    // We are awake, but not sure if we should be
    SerialDebug(1, "t_b_s %d\r\n",
		(int)tries_before_sleep);
    if (tries_before_sleep == -1) {
      // First time
      tries_before_sleep = 4;
      SerialDebug(1, "t_b_s=>4\r\n");
    }
    if (have_a_live_key) {
      SerialDebug(1, "send USB res\r\n");
      pending_key_wake = 0;
      do_suspend = 1; // So we don't stay awake if the host is down
      // Must set this before we call SendResume or it gets set after
      // it gets cleared in WakeUp
      _delay_us(300);
      USBCBSendResume();
    } else {
      tries_before_sleep--;
      if (tries_before_sleep < 1) {
	pending_key_wake = 0;
	SerialDebug(1, "back to sleep\r\n");
	tries_before_sleep = -1;
	do_suspend = 1;
      }
    }
  }
  queueReport();
}
