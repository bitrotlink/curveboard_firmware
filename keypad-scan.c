#include <avr/io.h>
#include <util/delay.h>
#include <inttypes.h>
#include <stdio.h>

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

#define strbuflen 64
unsigned char sbuf[strbuflen];

unsigned char kp_line; // Array of 8 bits, for keypad line state.
unsigned char kp[8]; // Array of 64 bits, for 8*8 matrix of keypad state (all lines).
unsigned char new_kp[8]; // New keypad state.

int cmp_array(char* x, char* y) {
	int i;
	for(i=0;i<8;i++) if(x[i]!=y[i]) return -1;
	return 0;
}

void cpy_array(char* src, char* dst) {
	int i;
	for(i=0;i<8;i++) dst[i]=src[i];
}

unsigned char read_kp_line() { // Return new keypad line state.
	return (PIND&0xfc)|(PINB&3);
}

void scan_kp() {
	int i; int line;
	for(i=0; i<8; i++) { // Drive each line, one at a time, and read all lines after each drive.
		line=1<<i;
		DDRD=(DDRD&3)|(line&0xfc); // Set all but the selected line to input.
		DDRB=(DDRB&0xfc)|(line&3);
		//Unneeded: PORTD|=0xfc; PORTB|=3; // Enable all the pullup resistors.
		PORTD=(PORTD&3)|((~line)&0xfc); // Drive the selected line low, and enable pullup resistors on all the input lines.
		PORTB=(PORTB&0xfc)|((~line)&3);
		_delay_ms(1); //debug
		new_kp[i]=read_kp_line();
	}
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

void uartWriteString(char* s) {
	int i;
	for(i=0; i<strbuflen; i++) {
		if(s[i]=='\0') break;
		USARTWriteChar(s[i]);
	}
}

void clearscreen(void) {
			sprintf(sbuf, "%c[2J\r", 0x1b);
			uartWriteString(sbuf);
}

void print_kp (void) {
	int i; int j;
	clearscreen();
	for(i=0; i<8; i++) {
		for(j=0; j<8; j++) {
			if(i==j) sbuf[j]='_'; // Driven pin is always low; mark as '_' instead of '0' for clarity.
			else if((kp[i])&(1<<j)) sbuf[j]='1';
			else sbuf[j]='0';
		}
		sprintf(sbuf+8, "\r\n");
		uartWriteString(sbuf);
	}
}

int main (void)
{
	//DDRD|=0xfc; DDRB|=1;
	int i;
	USARTInit(0); //416 is 2400bps, 0 is 1Mbps. Can't do 115200bps reliably because UART clock is divided from the arduino uno's 16MHz master clock, which can't get near enough to 115.2kHz. See table 20-6 on page 200 of http://www.atmel.com/Images/doc8271.pdf for details.
	unsigned char c;
	//_delay_ms(2000); //debug
	c=USARTReadChar();
	sprintf(sbuf, "Starting...");
	uartWriteString(sbuf);
	_delay_ms(1000); //debug
	while(1) {
		scan_kp();
		if(cmp_array(kp, new_kp)) {
			cpy_array(new_kp, kp);
			print_kp();
			_delay_ms(10); //shitty debounce
		}
	}

	while(1) {
		c=USARTReadChar();
		if(c=='a') USARTWriteChar('\n');
		else if(c=='d') USARTWriteChar('\r');
		else if (c=='c') clearscreen();
		else USARTWriteChar(c);
	}
	return 0;
}
