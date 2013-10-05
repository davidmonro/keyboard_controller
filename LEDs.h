// Copyright 2013 David Monro davidm@davidmonro.net

// Define which LEDs are on what pin of which port

// We have no less than 9 LEDS right now
// 5 for the keyboard, 4 for debugging
// The 4 for debugging are expected to be pins 4-7 of a port


#if defined(__AVR_ATmega32U4__)

#define NUMLEDPIN  PIND0
#define NUMLEDPORT PORTD
#define NUMLEDDDR  DDRD

#define CAPSLEDPIN  PIND1
#define CAPSLEDPORT PORTD
#define CAPSLEDDDR  DDRD

#define SCRLLEDPIN  PIND2
#define SCRLLEDPORT PORTD
#define SCRLLEDDDR  DDRD

#define COMPLEDPIN  PINF0
#define COMPLEDPORT PORTF
#define COMPLEDDDR  DDRF

#define KANALEDPIN  PINF1
#define KANALEDPORT PORTF
#define KANALEDDDR  DDRF

#define DEBUGLEDPORT PORTF
#define DEBUGLEDDDR  DDRF

#endif

#if defined(__AVR_ATmega32U2__)

#define NUMLEDPIN  PIND0
#define NUMLEDPORT PORTD
#define NUMLEDDDR  DDRD

#define CAPSLEDPIN  PIND1
#define CAPSLEDPORT PORTD
#define CAPSLEDDDR  DDRD

#define SCRLLEDPIN  PIND2
#define SCRLLEDPORT PORTD
#define SCRLLEDDDR  DDRD

#define COMPLEDPIN  PINC0
#define COMPLEDPORT PORTC
#define COMPLEDDDR  DDRC

#define KANALEDPIN  PINC1
#define KANALEDPORT PORTC
#define KANALEDDDR  DDRC

#define DEBUGLEDPORT PORTC
#define DEBUGLEDDDR  DDRC

#endif
