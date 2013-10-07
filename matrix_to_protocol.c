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
static uint8_t oldreport1[REPORT1LENGTH];
static uint8_t newreport1[REPORT1LENGTH];

#define REPORT2LENGTH 2
static uint8_t oldreport2[REPORT2LENGTH];
static uint8_t newreport2[REPORT2LENGTH];

static uint8_t currkey = 0;
static uint8_t rollover = 0;
static uint8_t have_a_live_key = 0;

// Ringbuffer stuff
#define RINGBUFFERSLOTS ((uint8_t)(8))
static uint8_t report1RingBuffer[RINGBUFFERSLOTS * REPORT1LENGTH];

static uint8_t report2RingBuffer[RINGBUFFERSLOTS * REPORT2LENGTH];

typedef struct {
		uint8_t *oldreport;
		uint8_t *newreport;
		uint8_t *reportRingBuffer;
		uint8_t rbHeadIndx;
		uint8_t rbTailIndx;
		uint8_t reportlength;
} reportdata_t;

static reportdata_t rep1data = {oldreport1,newreport1, report1RingBuffer, 0, 0, REPORT1LENGTH};
static reportdata_t rep2data = {oldreport2,newreport2, report2RingBuffer, 0, 0, REPORT2LENGTH};

void queueReport(uint8_t qnum)
{
  reportdata_t *rd;

  if (qnum == 1) {
    rd = &rep1data;
  } else {
    rd = &rep2data;
  }
  SerialDebug(10, "enq q%d %d\r\n", qnum, rd->rbTailIndx);
   
    // Only bother enqueueing it if it is different to the last one
  if (memcmp(rd->oldreport, rd->newreport, rd->reportlength) != 0) {
    if ((((rd->rbTailIndx) + 1) % RINGBUFFERSLOTS) != (rd->rbHeadIndx)) {
      memcpy(rd->reportRingBuffer + rd->reportlength * (rd->rbTailIndx),
	rd->newreport, rd->reportlength);
      SerialDebug(2, "enq q%d %d\r\n", qnum, rd->rbTailIndx);
      (rd->rbTailIndx) ++;
      (rd->rbTailIndx) %= RINGBUFFERSLOTS;
    } else {
      SerialDebug(1, "QFULL h %d t %d\r\n", rd->rbHeadIndx,
	rd->rbTailIndx);
    }
    // even if we didn't queue it, no point getting into a loop trying to report it
    memcpy(rd->oldreport, rd->newreport, rd->reportlength);
  }
}

void *dequeueReport(uint8_t qnum)
{
  void* ret;
  reportdata_t *rd;

  if (qnum == 1) {
    rd = &rep1data;
  } else {
    rd = &rep2data;
  }
  SerialDebug(10, "dq q%d %d", qnum, rd->rbHeadIndx);

  if (rd->rbHeadIndx == rd->rbTailIndx) {
    SerialDebug(10, " queue was empty\r\n");
    return NULL;
  }
  // We know there's something in the queue
  ret = rd->reportRingBuffer + rd->reportlength * rd->rbHeadIndx;
  SerialDebug(2, "dq q%d %d ok\r\n", qnum, rd->rbHeadIndx);
  (rd->rbHeadIndx)++;
  (rd->rbHeadIndx) %= RINGBUFFERSLOTS;
  return ret;
}

void initScanProcessor(void)
{
  memset(rep1data.oldreport, 0, rep1data.reportlength);
  memset(rep1data.newreport, 0, rep1data.reportlength);
  memset(rep1data.reportRingBuffer, 0, RINGBUFFERSLOTS * rep1data.reportlength);
  rep1data.rbHeadIndx = 0;
  rep1data.rbTailIndx = 0;

  memset(rep2data.oldreport, 0, rep2data.reportlength);
  memset(rep2data.newreport, 0, rep2data.reportlength);
  memset(rep2data.reportRingBuffer, 0, RINGBUFFERSLOTS * rep2data.reportlength);
  rep2data.rbHeadIndx = 0;
  rep2data.rbTailIndx = 0;
}

void beginScanEvent(void)
{
  // A scan is beginning.
  memset(rep1data.newreport, 0, rep1data.reportlength);
  memset(rep2data.newreport, 0, rep2data.reportlength);
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
	rep1data.newreport[2 + currkey] = key;
	currkey++;
	if (currkey > 5) {
	  currkey = 5;
	  rollover = 1;
	}
      } else {
	// Modifiers are 0xE0 to 0xE7 and map 1:1 to bits 0:7 of the first
	// byte of the report
	rep1data.newreport[0] |= (1 << (key & 0x0f));
      }
    } else {
      // Special key not in normal range
      switch(scanpoint) {
	case 0x2d:
	  rep2data.newreport[0] |= 0x10;
	  break;
	case 0x0d:
	  rep2data.newreport[0] |= 0x20;
	  break;
	case 0x11:
	  rep2data.newreport[0] |= 0x04;
	  break;
	case 0x1b:
	  rep2data.newreport[1] |= 0x02;
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
    memset(rep1data.newreport + 2, 0x01, 6);
    // Don't think we can do anything with report2
    // So just copy the old report.
    memcpy(rep2data.newreport, rep2data.oldreport, rep2data.reportlength);
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
