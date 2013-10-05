// Copyright 2013 David Monro davidm@davidmonro.net

#include <avr/io.h>

#define BAUD 921600
#include <util/setbaud.h>

void uartInit(void)
{

   // Set baud rate using the values defined through the setbaud.h include
   UBRR1H = UBRRH_VALUE;
   UBRR1L = UBRRL_VALUE;

   #if USE_2X
   UCSR1A |= _BV(U2X1);
   #else
   UCSR1A &= ~(_BV(U2X1));
   #endif

   UCSR1B = _BV(TXEN1);
   UCSR1C = _BV(UCSZ11) | _BV(UCSZ10);
}

void uartPutchar(char c)
{

  while( ! (UCSR1A & _BV(UDRE1))); // busy wait
  UDR1 = c;
}

void uartPuts(char *str)
{
  uint8_t i = 0; // Limit of 256 chars!
  while (str[i]) {
	uartPutchar(str[i++]);
  }
}
