#include <avr/io.h>
#include <util/delay.h>

//Copied from http://www.protostack.com/blog/2011/02/analogue-to-digital-conversion-on-an-atmega168/
uint16_t ReadADC(uint8_t __channel)
{
   ADMUX |= __channel;                // Channel selection
   ADCSRA |= _BV(ADSC);               // Start conversion
   while(!bit_is_set(ADCSRA,ADIF));   // Loop until conversion is complete
   ADCSRA |= _BV(ADIF);               // Clear ADIF by writing a 1 (this sets the value to 0)

   return(ADC);
}

void adc_init()
{
        ADCSRA = _BV(ADEN) | _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0); //Enable ADC and set 128 prescale
}

void write_7seg(int i) {
	int ss_bitmap;
	switch(i) {
		case 0: ss_bitmap=0x7d; break;
		case 1: ss_bitmap=0x11; break;
		case 2: ss_bitmap=0x3e; break;
		case 3: ss_bitmap=0x37; break;
		case 4: ss_bitmap=0x53; break;
		case 5: ss_bitmap=0x67; break;
		case 6: ss_bitmap=0x6f; break;
		case 7: ss_bitmap=0x31; break;
		case 8: ss_bitmap=0x7f; break;
		case 9: ss_bitmap=0x77; break;
		case 0xa: ss_bitmap=0x7b; break;
		case 0xb: ss_bitmap=0x4f; break;
		case 0xc: ss_bitmap=0x6c; break;
		case 0xd: ss_bitmap=0x1f; break;
		case 0xe: ss_bitmap=0x6e; break;
		case 0xf: ss_bitmap=0x6a; break;
		default: ss_bitmap=0x26; // error
	}
	// segment testing: ss_bitmap=1<<i;
	ss_bitmap^=0xff; // Active low (display is common anode).
	PORTD=(PORTD&3)|((ss_bitmap&0x3f)<<2); // Lower 6 segments are D bits 2-7.
	PORTB=(PORTB&0xfe)|((ss_bitmap&0x40)>>6); // Highest segment is B bit 0.
}

int main (void)
{
	adc_init();
	DDRD|=0xfc; DDRB|=1;
	int i;
	while(1) {
		//for(i=0; i<17; i++) {
		//	write_7seg(i);
		//	_delay_ms(1000);
		//}
		i=ReadADC(0);
		i>>=6;
		write_7seg(i);
	}
	return 0;
}
