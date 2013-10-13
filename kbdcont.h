// Copyright 2013 David Monro davidm@davidmonro.net
// Some defines, debugging stuff, and a heap of extern defs...

#ifndef KBDCONT_H
#define KBDCONT_H


#include <stdio.h>
#include <string.h>
#include <avr/io.h>
#include <util/delay.h>

// What does our keyboard matrix look like?
// If MATRIXCOLS*MATRIXROWS exceeds 255, you'll need to do some work on
// the types used in the scanning loops and probably change the uint8_t
// casts on these definitions to unsigned short int.

// Columns are those things which we strobe
// If this exceeds 256 things will break all over the place
#define MATRIXCOLS ((uint8_t)(16))

// Rows are those things we read the results from
// This needs to be the length of an integer of some sort, and you need
// to typedef a kbdrow_t to an unsigned type of the right length.
// If you have a funny number of rows round this up and just ignore
// those bits (set to 0 on kbdSampleRows, 0x00 entries in keymap.h
#define MATRIXROWS ((uint8_t)(8))
typedef uint8_t kbdrow_t;

// This needs to be big enough to hold your scan values.
typedef uint8_t kbdscanval_t;

// Debugging
#define SERIALDEBUG 1

#ifdef SERIALDEBUG
extern void SerialDebug2(const char *fmt, ...);
#define SerialDebug(level, ...) {if (level <= SERIALDEBUG) { SerialDebug2(__VA_ARGS__); }}
#else
#define SerialDebug(level, ...) do {} while(0)
#endif

// Exports from main.c
extern volatile uint8_t do_suspend;
extern volatile uint8_t pending_key_wake;
extern volatile uint8_t was_usb_wake;
extern unsigned long next_idle_retrans;
extern volatile unsigned long getMillis(void);

extern unsigned long suppress_expire;
extern uint8_t do_suppress;

// From keyboard_matrix.c
extern void initKeyboardMatrix(void);
extern void doKeyboardScan(void);

// From matrix_to_protocol.c
extern void initScanProcessor(void);
extern void beginScanEvent(void);
extern void addScanPoint(kbdscanval_t scanpoint);
extern void invalidateScan(void);
extern void endScanEvent(void);
// extern void sendReport(uint8_t);
extern void queueReport(uint8_t);
extern void *dequeueReport(uint8_t);

// From hardware_interface.c
extern void initKeyboardHardware(void);
extern void kbdActivateColumn(uint8_t pin);
extern void kbdActivateAllColumns(void);
extern kbdrow_t kbdSampleRows(void);
extern void kbdInitInterruptSources(void);
extern void kbdConfigureInterruptsForSleep(void);
extern void kbdConfigureInterruptsForWake(void);
extern void kbdSetLeds(uint8_t ledstate);
extern void kbdSetExtraLeds(uint8_t ledstate);
extern void debugLedOn(uint8_t led);
extern void debugLedOff(uint8_t led);
extern void debugLedToggle(uint8_t led);

// From usb_interface.c
extern uint8_t USBSendReport(unsigned char *);
extern void USBCBSendResume(void);
extern uint8_t USB_Not_Active(void);
extern void USB_Init(void);
extern void USB_Poll(void);
extern void USB_Check_For_Input(void);
extern void USBDeviceTasks(void);
extern uint8_t Get_USB_DeviceState(void);

// From uart.c
void uartInit(void);
void uartPutchar(char c);
void uartPuts(char *str);

// Other stuff

// These are from usb_function_hid.c in the MAL
extern uint8_t idle_rate;
extern uint8_t active_protocol;

#endif
