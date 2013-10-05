// Copyright 2013 David Monro davidm@davidmonro.net

// Driving a pair of cascaded 74HC595
// Configured with PD4 as data clock, PD5 as data, PD6 as latch
// data is sampled on the rising edge of the data clock
// 

#include <avr/io.h>
#include <util/delay_basic.h>


#define SRPORT (PORTD)
#define SRPORTCTL (DDRD)
#define SR_DC (PIND4)
#define SR_DS (PIND5)
#define SR_LC (PIND6)

// Initialize with clock low, data low, latch low
void srInit(void)
{
	// Set the outputs to low and the pins to output
	SRPORT &= ~((_BV(SR_DC) | _BV(SR_DS) | _BV(SR_LC) ));
	SRPORTCTL |= (_BV(SR_DC) | _BV(SR_DS) | _BV(SR_LC) );
}

void srWrite(uint16_t data)
{
	uint8_t i;

	for (i=0; i< 16; i++) {
		// Set up the data line
		if (data & 0x1) {
			SRPORT |= _BV(SR_DS);
		} else {
			SRPORT &= ~(_BV(SR_DS));
		}
		_delay_loop_1(1); // Settling time
		// Raise the data clock
		SRPORT |= _BV(SR_DC);
		_delay_loop_1(1); // hold time
		// Lower the data clock
		SRPORT &= ~(_BV(SR_DC));
		data >>=1;

	}
	// All data transmitted, raise the latch clock
	SRPORT |= _BV(SR_LC);
	_delay_loop_1(1); // hold time
	// Lower the latch clock
	SRPORT &= ~(_BV(SR_LC));
}
