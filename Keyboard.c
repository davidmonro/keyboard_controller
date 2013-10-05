// Copyright 2013 David Monro davidm@davidmonro.net

// The main file.
// Actually I shoudl abstract out the AVR-specific bits oneday...

#include "kbdcont.h"
#include <stdarg.h>
#include <inttypes.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/io.h>
#include <util/atomic.h>


// Globals
volatile unsigned long millis = 0;
volatile uint8_t do_suspend = 0;
volatile uint8_t pending_key_wake = 0;
volatile uint8_t was_usb_wake = 0;
char realoutstr[70];
char *outstr;

unsigned long next_idle_retrans = 500;

unsigned long suppress_expire = 0;
uint8_t do_suppress = 0;

// Locals
volatile static uint8_t was_sleeping = 0;

// Interrupt service routines

// Fires once per millisecond
ISR(TIMER0_COMPA_vect)
{
  millis++;
}


// Fires if any keyboard row changes
ISR(PCINT0_vect)
{
  SerialDebug(3, "PINCHG\r\n");
}

// Catchall
ISR(BADISR_vect)
{
  SerialDebug(1, "unk int\r\n");
}

#ifdef SERIALDEBUG
// Debugging
void SerialDebug2(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  vsprintf(outstr, fmt, args);
  uartPuts(outstr);
  va_end(args);
}
#endif


void goToSleep(void)
{
  SerialDebug(3, "sleeping\r\n");

  // Only do it once...
  do_suspend = 0;

  // Clear these, one will be set later
  pending_key_wake = 0;
  was_usb_wake = 0;

  // prepare the hardware
  kbdActivateAllColumns();
  kbdConfigureInterruptsForSleep();

  // housekeeping
  was_sleeping = 1;

  // Actually do it
  debugLedOff(3);
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_mode();

  // And now we are back
  debugLedOn(3);

  if (was_usb_wake) {
    SerialDebug(1, "WAKE:usb\r\n");
    was_usb_wake = 0;
    pending_key_wake = 0;
  } else {
    SerialDebug(1, "WAKE:key\r\n");
    // If we got woken by a row interrupt, we need to see
    // if there really was a keypress
    pending_key_wake = 1;
  }

  do_suppress = 1; // Always pause a bit after waking
  suppress_expire = (unsigned long)getMillis() + 100UL;
}

volatile unsigned long int getMillis(void)
{
  unsigned long int localcopy;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
  {
	  localcopy = millis;
  }
  return localcopy;
}

void initInterrupts(void)
{
  sei();
  SerialDebug(2, "SREG %02x\r\n", SREG);
}

void timerInit(void)
{
  // Use OCR0A for count (wg mode 2), use a 64 prescaler
  TCCR0A = _BV(WGM01);
  TCCR0B = _BV(CS01) | _BV(CS00);
  // Set the ICR1 reg for 1ms count
  OCR0A = 250;
  // Set the top interrupt enable
  TIMSK0 = _BV(OCIE0A);
}

int main(void)
{
  //int count = 0;
  unsigned long int nextscan = 0;
  unsigned long int nextprint = 0;
  outstr = realoutstr;		// declaration issue

  timerInit(); // interrupts every ms

#ifdef SERIALDEBUG
  uartInit();
#endif

  SerialDebug(2, "Hello World\r\n");
  debugLedOn(3);

  // Get the rest of the hardware set up
  initKeyboardHardware();
  initKeyboardMatrix();
  initScanProcessor();

  // And now the interrupt layer
  kbdInitInterruptSources();
  initInterrupts();

  // Wait 100ms for the bus to settle down
  while (1) {
    unsigned long now = getMillis();
    if ((signed long)now - (signed long)100 >= 0) {
      break;
    }
  }

  // This will enable the USB interrupt as well
  USB_Init();

  // Main loop
  while (1) {
    unsigned long now = getMillis();

    // Toggle an LED and optionally print a message every second
    if ((signed long)now - (signed long)nextprint >= 0) {
      SerialDebug(3,
		  "now %lu, idlr %d, prt %d, nxti = %lu\r\n",
		  now, (int)idle_rate, (int)active_protocol,
		  next_idle_retrans);
      debugLedToggle(0);
      nextprint = now + 1000;
    }

    // We do a scan every 5ms
    if ((signed long)now - (signed long)nextscan >= 0) {
      nextscan = now + 5;
      doKeyboardScan();
    }

    // Needs to be run often
    USB_Poll();

    // if the usb bus goes quiet...
    if (do_suspend) {
      goToSleep();
    }


    // Idle mode leaves all the timers running, so we will sleep at most 1ms
    set_sleep_mode(SLEEP_MODE_IDLE);
    sleep_mode();
  }
}
