#include <avr/io.h>
#include <util/delay.h>
#include <inttypes.h>

//Copied from http://www.avrfreaks.net/index.php?name=PNphpBB2&file=printview&t=48331&start=0
#define    UCSRA    UCSR0A
#define    UCSRB    UCSR0B
#define    UCSRC    UCSR0C
#define    UBRRH    UBRR0H
#define    UBRRL    UBRR0L
#define    UDRE    UDRE0
#define    UDR    UDR0
#define    RXC    RXC0

//I wrote:
#define UBRR UBRR0
#define RXEN RXEN0 
#define TXEN TXEN0
#define UCSZ0 UCSZ00

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

//Copied from http://extremeelectronics.co.in/avr-tutorials/using-the-usart-of-avr-microcontrollers/ (and superfluous whitespace removed)
void USARTInit(uint16_t ubrr_value) {
	UBRR= ubrr_value; //Set Baud rate
	//Original source said (apparently with "#define URSEL 7" somewhere):
	//UCSRC=(1<<URSEL)|(3<<UCSZ00); //Set Frame Format: Asynchronous mode, No Parity, 1 StopBit, char size 8
	UCSRC=(3<<UCSZ0); //Fixed for 328P.
	UCSRB=(1<<RXEN)|(1<<TXEN); //Enable The receiver and transmitter
}

char USARTReadChar() {
	while(!(UCSRA & (1<<RXC))); //Wait until a data is available
	return UDR; //Now USART has got data from host and is available is buffer
}

void USARTWriteChar(char data) {
	while(!(UCSRA & (1<<UDRE))); //Wait until the transmitter is ready
	UDR=data; //Now write the data to USART buffer
}

//I wrote this
void write_7seg(int i, int enable) {
	int ss_bitmap;
	if(!enable) ss_bitmap=0; // Turn off all segments.
	else switch(i) {
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
		default: ss_bitmap=0x26; // Greek Xi.
	}
	//debug: individual segment testing: ss_bitmap=1<<i;
	ss_bitmap^=0xff; // Active low (display is common anode).
	PORTD=(PORTD&3)|((ss_bitmap&0x3f)<<2); // Lower 6 segments are D bits 2-7.
	PORTB=(PORTB&0xfe)|((ss_bitmap&0x40)>>6); // Highest segment is B bit 0.
}

int main (void)
{
	adc_init();
	DDRD|=0xfc; DDRB|=1;
	int i;
	USARTInit(0); //416 is 2400bps, 0 is 1Mbps. Can't do 115200bps reliably because UART clock is divided from the arduino uno's 16MHz master clock, which can't get near enough to 115.2kHz. See table 20-6 on page 200 of http://www.atmel.com/Images/doc8271.pdf for details.
	unsigned char c;
	write_7seg(0x10, 1); //Debug; write error signal to indiate progress.
	while(1) {
		c=USARTReadChar();
		USARTWriteChar(c);
		write_7seg(c&0xf, 1);
		/* Debug:
		write_7seg(0, 0); _delay_ms(250);
		write_7seg(c>>4, 1); _delay_ms(500);
		write_7seg(0, 0); _delay_ms(250);
		write_7seg(c&0xf, 1); _delay_ms(500);
		write_7seg(0, 0); _delay_ms(250);
		write_7seg(0x10, 1); _delay_ms(500);
		*/
	}
	/* while(1) {
		i=ReadADC(0);
		i>>=6;
		write_7seg(i, 1);
	} */
	/* while(1) {
		for(i=0; i<17; i++) {
			write_7seg(i, 1);
			_delay_ms(1000);
		}
	} */
	return 0;
}
