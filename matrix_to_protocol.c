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

#define REPORT1LENGTH 8
static unsigned char oldreport1[REPORT1LENGTH];
static unsigned char newreport1[REPORT1LENGTH];

#define REPORT2LENGTH 2
static unsigned char oldreport2[REPORT2LENGTH];
static unsigned char newreport2[REPORT2LENGTH];

static uint8_t currkey = 0;
static uint8_t rollover = 0;
static uint8_t have_a_live_key = 0;

// Ringbuffer stuff
#define RINGBUFFERSLOTS ((uint8_t)(8))
static uint8_t report1RingBuffer[RINGBUFFERSLOTS * REPORT1LENGTH];
static uint8_t rb1HeadIndx, rb1TailIndx;

static uint8_t report2RingBuffer[RINGBUFFERSLOTS * REPORT2LENGTH];
static uint8_t rb2HeadIndx, rb2TailIndx;


void queueReport(uint8_t qnum)
{
  void *oldreport,*newreport,*reportRingBuffer;
  int reportlength;
  uint8_t *rbTailIndx, *rbHeadIndx;

  if (qnum == 1) {
    reportRingBuffer = report1RingBuffer;
    oldreport = oldreport1;
    newreport = newreport1;
    reportlength = REPORT1LENGTH;
    rbTailIndx = &rb1TailIndx;
    rbHeadIndx = &rb1HeadIndx;
  } else {
    reportRingBuffer = report2RingBuffer;
    oldreport = oldreport2;
    newreport = newreport2;
    reportlength = REPORT2LENGTH;
    rbTailIndx = &rb2TailIndx;
    rbHeadIndx = &rb2HeadIndx;
  }
  SerialDebug(10, "enq q%d %d\r\n", qnum, (*rbTailIndx));
   
    // Only bother enqueueing it if it is different to the last one
  if (memcmp((void *)oldreport, (void *)newreport, reportlength) != 0) {
    if ((((*rbTailIndx) + 1) % RINGBUFFERSLOTS) != (*rbHeadIndx)) {
      memcpy(reportRingBuffer + reportlength * (*rbTailIndx),
	(void *)newreport, reportlength);
      SerialDebug(2, "enq q%d %d\r\n", qnum, (*rbTailIndx));
      (*rbTailIndx)++;
      (*rbTailIndx) %= RINGBUFFERSLOTS;
    } else {
      SerialDebug(1, "QFULL h %d t %d\r\n", (int)(*rbHeadIndx),
	(int)(*rbTailIndx));
    }
    // even if we didn't queue it, no point getting into a loop trying to report it
    memcpy(oldreport, (void *)newreport, reportlength);
  }
}

void *dequeueReport(uint8_t qnum)
{
  void* ret;

  void *reportRingBuffer;
  int reportlength;
  uint8_t *rbTailIndx, *rbHeadIndx;

  if (qnum == 1) {
    reportRingBuffer = report1RingBuffer;
    reportlength = REPORT1LENGTH;
    rbTailIndx = &rb1TailIndx;
    rbHeadIndx = &rb1HeadIndx;
  } else {
    reportRingBuffer = report2RingBuffer;
    reportlength = REPORT2LENGTH;
    rbTailIndx = &rb2TailIndx;
    rbHeadIndx = &rb2HeadIndx;
  }
  SerialDebug(10, "dq q%d %d", qnum, *rbHeadIndx);

  if ((*rbHeadIndx) == (*rbTailIndx)) {
    SerialDebug(10, " queue was empty\r\n");
    return NULL;
  }
  // We know there's something in the queue
  ret = reportRingBuffer + reportlength * (*rbHeadIndx);
  SerialDebug(2, "dq q%d %d ok\r\n", qnum, (*rbHeadIndx));
  (*rbHeadIndx)++;
  (*rbHeadIndx) %= RINGBUFFERSLOTS;
  return ret;
}

void initScanProcessor(void)
{
  memset(oldreport1, 0, REPORT1LENGTH);
  memset(newreport1, 0, REPORT1LENGTH);
  memset(oldreport2, 0, REPORT2LENGTH);
  memset(newreport2, 0, REPORT2LENGTH);

  memset(report1RingBuffer, 0, RINGBUFFERSLOTS * REPORT1LENGTH);
  rb1HeadIndx = 0;
  rb1TailIndx = 0;
  memset(report2RingBuffer, 0, RINGBUFFERSLOTS * REPORT2LENGTH);
  rb2HeadIndx = 0;
  rb2TailIndx = 0;
}

void beginScanEvent(void)
{
  // A scan is beginning.
  memset(newreport1, 0, REPORT1LENGTH);
  memset(newreport2, 0, REPORT2LENGTH);
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
    if (key != 0x01) {
      if (key < 0xe0) {
	// Not a modifier
	newreport1[2 + currkey] = key;
	currkey++;
	if (currkey > 5) {
	  currkey = 5;
	  rollover = 1;
	}
      } else {
	// Modifiers are 0xE0 to 0xE7 and map 1:1 to bits 0:7 of the first
	// byte of the report
	newreport1[0] |= (1 << (key & 0x0f));
      }
    } else {
      // Special key not in normal range
      switch(scanpoint) {
	case 0x2d:
	  newreport2[0] |= 0x10;
	  break;
	case 0x0d:
	  newreport2[0] |= 0x20;
	  break;
	case 0x11:
	  newreport2[0] |= 0x40;
	  break;
	case 0x1b:
	  newreport2[1] |= 0x02;
	  break;
	default:
          SerialDebug(1, "UNK SP %02x\r\n", scanpoint);
	  break;
      }
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
    memset(newreport1 + 2, 0x01, 6);
    // Don't think we can do anything with report2
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
  queueReport(1);
  queueReport(2);
}
