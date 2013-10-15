// Copyright 2013 David Monro davidm@davidmonro.net
// This is where you define all the functions which interact directly with the
// hardware

#include "kbdcont.h"
#include "shiftreg.h"
#include "LEDs.h"
#include <util/delay.h>

void kbdActivateColumn(uint8_t pin)
{
  // This function enables a single column on the keyboard matrix (specified
  // by the "pin" argument) and disables all other columns.
  // Note: active LOW

  uint16_t data = 1 << pin;
  srWrite(~(data));
  _delay_us(1);
}

void kbdActivateAllColumns()
{
  // This function enables all columns on the keyboard matrix simultaneously.
  // The idea is that with all columns enabled, _any_ key being pressed will
  // show up on the row inputs

  // This feature is used to short-circuit the scanning loop, and also to
  // prepare for sending the chip to sleep to wait on interrupts
  srWrite(0);
  _delay_us(1);
}

void initKeyboardHardware()
{
  // This function sets up the IO ports for the keyboard hardware
  DDRB = 0x00; // port B is all input
  PORTB = 0xff; // pullups enabled

  srInit();

  // It also needs to handle the LEDS...

  NUMLEDPORT &= ~(_BV(NUMLEDPIN));
  NUMLEDDDR |= _BV(NUMLEDPIN);

  CAPSLEDPORT &= ~(_BV(CAPSLEDPIN));
  CAPSLEDDDR |= _BV(CAPSLEDPIN);

  SCRLLEDPORT &= ~(_BV(SCRLLEDPIN));
  SCRLLEDDDR |= _BV(SCRLLEDPIN);

  COMPLEDPORT &= ~(_BV(COMPLEDPIN));
  COMPLEDDDR |= _BV(COMPLEDPIN);

  KANALEDPORT &= ~(_BV(KANALEDPIN));
  KANALEDDDR |= _BV(KANALEDPIN);

  DEBUGLEDPORT &= 0x0f;
  DEBUGLEDDDR |= 0xf0;
}

kbdrow_t kbdSampleRows(void)
{
  // This function returns a bitmask of the active rows. This implementation
  // simply uses all of PORTB for the rows, but you could reduce pincount (or
  // handle bigger matrices) by using shift registers etc

  // Active LOW, so invert.
  return (PINB ^ 0xFF);
}

void kbdInitInterruptSources(void)
{
  // Here we do the initial setup for whatever is needed for generation of
  // interrupts on a row becoming active during sleep. There are separate
  // functions for enabling them and disabling them upon entry to and exit
  // from sleep (assuming you need them).

  // You need to make sure that with all columns activated, _any_ key being
  // pressed will generate an interrupt. If you are using shift registers or
  // something on row input, you might need something like an n-input AND gate
  // (assuming active low) tied to an INT-capable pin.

  PCICR |= _BV(PCIE0);
  PCMSK0 = 0xff; // Enable all of them

}

void kbdConfigureInterruptsForSleep()
{
  // In my implementation, nothing to do here. If you need to configure
  // something differently between waking and sleeping modes, do it here
}

void kbdConfigureInterruptsForWake()
{
  // In my implementation, nothing to do here. If you need to configure
  // something differently between sleeping and waking modes, do it here
}


void kbdSetLeds(uint8_t ledstate)
{
  // LEDS should be NUM, CAPS, SCROLL, COMPOSE, KANA
  // In theory we should be able to get Power, Shift and
  // Do Not Disturb, but I don't think the linux input driver
  // can handle those.

  // The input layer does handle other LEDS with higher usage
  // codes, but that would need another report format. TBD.

  SerialDebug(1, "kbdSetLeds %02x\r\n", (int)ledstate);

  // Assignment for now:
  // NUM: D0
  // CAPS: D1
  // SCROLL: D2
  // COMPOSE: F0
  // KANA: F1

  if (ledstate & 0x01) {
    NUMLEDPORT |= _BV(NUMLEDPIN);
  } else  {
    NUMLEDPORT &= ~(_BV(NUMLEDPIN));
  }

  if (ledstate & 0x02) {
    CAPSLEDPORT |= _BV(CAPSLEDPIN);
  } else  {
    CAPSLEDPORT &= ~(_BV(CAPSLEDPIN));
  }

  if (ledstate & 0x04) {
    SCRLLEDPORT |= _BV(SCRLLEDPIN);
  } else  {
    SCRLLEDPORT &= ~(_BV(SCRLLEDPIN));
  }

  if (ledstate & 0x08) {
    COMPLEDPORT |= _BV(COMPLEDPIN);
  } else  {
    COMPLEDPORT &= ~(_BV(COMPLEDPIN));
  }

  if (ledstate & 0x10) {
    KANALEDPORT |= _BV(KANALEDPIN);
  } else  {
    KANALEDPORT &= ~(_BV(KANALEDPIN));
  }
}

void debugLedOn(uint8_t led) {
    DEBUGLEDPORT |= 1 << (led+4);
}

void debugLedOff(uint8_t led) {
    DEBUGLEDPORT &= ~(1 << (led+4));
}

void debugLedToggle(uint8_t led) {
    DEBUGLEDPORT ^= 1 << (led+4);
}

void kbdSetExtraLeds(uint8_t ledstate)
{
  /* These come in in the following mapping, from the Linux
   * kernel and our report structure:
   * 0x01 - LED_MUTE
   * 0x02 - LED_MAIL
   * 0x04 - LED_SLEEP
   * 0x08 - LED_MISC
   * 0x10 - LED_SUSPEND
   * 0x20 - LED_CHARGING
   * This isn't the order they are defined in <linux/input.h>,
   * it is ordered by usage number (since that allowed me to
   * coalesce usages 0x4b-0x4d into one report stanza).
   */
  SerialDebug(1, "kbdSetExtraLeds %02x\r\n", (int)ledstate);
  if (ledstate & 0x04) {
    debugLedOn(1);
  } else {
    debugLedOff(1);
  }
}

