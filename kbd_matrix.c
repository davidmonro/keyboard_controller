// Copyright 2013 David Monro davidm@davidmonro.net

// Code in this file is responsible for handling the matrix. Debouncing
// is done here, as is ghost key checking if needed.

#include "kbdcont.h"

// 4 buffers for debounce
static kbdrow_t activekeys[MATRIXCOLS * 4];

// Storage for debounced samples, needed for blocking ghosts
static kbdrow_t active_debounced_keys[MATRIXCOLS];

void initKeyboardMatrix(void)
{
  memset(activekeys, 0, MATRIXCOLS * 4 * sizeof(kbdrow_t));
  memset(active_debounced_keys, 0, MATRIXCOLS * sizeof(kbdrow_t));
}

uint8_t bitcount(kbdrow_t n)
{
  // HACKED - terminates at count=2
  uint8_t count = 0;
  while (n && count < 2) {
    count++;
    n &= (n - 1);
  }
  return count;
}

void doKeyboardScan(void)
{
  static unsigned short int currbase = 0;
  uint8_t colpin;
  uint8_t bitpos;
  uint8_t rollover = 0;
  kbdrow_t tmp;
  uint8_t j;

  memset(activekeys + currbase, 0, MATRIXCOLS * sizeof(kbdrow_t));
  memset(active_debounced_keys, 0, MATRIXCOLS * sizeof(kbdrow_t));

  // Tell the protocol layer we are intiating a scan
  beginScanEvent();

  // First set all outputs to 0 and see if anything is pressed at all
  kbdActivateAllColumns();

  if (kbdSampleRows() != 0) {
    // Something was pressed. Fill in the curractive array.
    for (colpin = 0; colpin < MATRIXCOLS; colpin++) {
      kbdActivateColumn(colpin);
      *(activekeys + currbase + colpin) = kbdSampleRows();
#if SERIALDEBUG >= 3
      if (*(activekeys + currbase + colpin) != 0x00) {
	SerialDebug2("AR %02x %02x\r\n", (int)colpin,
	  (int)(*(activekeys + currbase + colpin)));
      }
#endif
    }

  }
#if SERIALDEBUG >= 4
  for (colpin = 0; colpin < MATRIXCOLS; colpin++) {
    SerialDebug2("%02x %02x %02x %02x\r\n", (int)(*(activekeys + colpin)), (int)(*(activekeys + 16 + colpin)), (int)(*(activekeys + 32 + colpin)), (int)(*(activekeys + 48 + colpin)));
  }
#endif

  // This version uses logic ops to hopefully do it a lot quicker
  // Thanks to the Karnaugh map solver at http://www.ee.calpoly.edu/media/uploads/resources/KarnaughExplorer_1.html
  for (colpin = 0; colpin < MATRIXCOLS; colpin++) {
    kbdrow_t a = *(activekeys + colpin);
    kbdrow_t b = *(activekeys + MATRIXCOLS + colpin);
    kbdrow_t c = *(activekeys + MATRIXCOLS * 2 + colpin);
    kbdrow_t d = *(activekeys + MATRIXCOLS * 3 + colpin);
    active_debounced_keys[colpin] = ( (a&b) | (c&d) | (b&d) | (b&c) | (a&d) | (a&c) );
    if (active_debounced_keys[colpin]) {
      // at least one point was active in the row
      for (bitpos = 0; bitpos < MATRIXROWS; bitpos++) {
	if (active_debounced_keys[colpin] & 1 << bitpos) {
	  addScanPoint(colpin * MATRIXROWS + bitpos);
	  SerialDebug(2, "DbLiveK %02x (%02x %02x)\r\n", (int) (colpin * MATRIXROWS + bitpos), (int)colpin, (int)(1 << bitpos));
	}
      }
    }
  }
	

  // Blocked key processing
  // I'm sure this could be integrated into the scan above but it would make it more complex
  for (colpin = 0; colpin < MATRIXCOLS - 1; colpin++) {
    tmp = active_debounced_keys[colpin];

    if (tmp == 0) {
      continue;
    }
    if (bitcount(tmp) == 1) {
      continue;
    }
    // At least 2 bits set on the current row
    for (j = colpin + 1; j < MATRIXCOLS; j++) {
      if (bitcount(tmp & active_debounced_keys[j]) > 1) {
	rollover = 1;
      }
    }
  }

  if (rollover) {
    // Tell the protocol layer about it
    invalidateScan();
  }
  currbase += MATRIXCOLS;
  if (currbase >= MATRIXCOLS * 4) {
    currbase = 0;
  }
  // Protocol layer notification of end of scan run
  endScanEvent();
  return;
}
