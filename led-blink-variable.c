//Blink led slow or fast depending on whether B1 is pulled low.

#include <avr/io.h>
#include <util/delay.h>

int ledstate;
int longsleep;

void toggle_led_and_sleep() {
	//ledstate!=ledstate;
	if(ledstate==0) ledstate=1;
	else ledstate=0;
	if(ledstate) PORTB |= _BV(PORTB5);
	else PORTB &= ~_BV(PORTB5);
	if(longsleep) _delay_ms(2000);
	else _delay_ms(500);
}

int main (void)
{
	ledstate=0;
	DDRB |= _BV(DDB5); // set pin 5 of PORTB for output
	DDRB &= ~_BV(DDB1); // B1 to input
	PORTB |= _BV(PORTB1); // Enable B1 internal 20kohm pullup

	while(1) {
		if(PINB&_BV(PINB1)) longsleep=1;
		else longsleep=0;
		toggle_led_and_sleep();
	}
	return 0;
}
