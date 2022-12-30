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
		DDRD=(DDRD&3)|(line&0xfc); // Set all but the selected line to input. Since the selected line was previously an input, with the internal pullup resistor enabled, and the previously selected line was driven low, and AVR when switching an interally-pulled-up input to an output causes it to drive high, this might short a driven-high pin and a driven-low pin during the transition period if the keypad is wired wrong. This could be avoided by first setting all lines as inputs, rather than doing the transition atomically.
		DDRB=(DDRB&0xfc)|(line&3);
		//Unneeded because the next two lines accomplish it: PORTD|=0xfc; PORTB|=3; // Enable all the pullup resistors.
		PORTD=(PORTD&3)|((~line)&0xfc); // Drive the selected line low, and enable pullup resistors on all the input lines.
		PORTB=(PORTB&0xfc)|((~line)&3);
		_delay_ms(1); //Without this, the inter-line scanning runs fast enough that speed-intolerant breadboard wiring (or maybe it's the keypad itself) causes glitches, manifested by scan states not stabilizing when keys are pressed.
		//asm("nop"); // http://support.atmel.no/knowledgebase/avrstudiohelp/mergedProjects/JTAGICEmkII/mkII/Html/JTAGICE_mkII_Special_Considerations.htm says "A NOP instruction must be placed between the OUT and the IN instruction to ensure that the correct value is present in the PIN register." But putting in just this nop doesn't solve the problem which the 1ms delay above does solve, proving that the problem isn't originating in the uC itself, so the problem must be the breadboard wiring or the keypad itself.
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

/* KP buttons found to short the following lines (in both directions; thus no diodes):
1       037
2       137
3       237
4       047
5       147
6       247
7       057
8       157
9       257
*       067
0       167
#       267
So, lines 0-6 are for a standard matrix (0-2 run vertically, 3-6 horizontally), and line 7 is common for all buttons, which would allow reading the keypad without scanning (by permanently driving line 7, and reading 0-6) but at the expense of additional ghosting during multiple simultaneous button presses (beyond even the ghosting of a traditional diode-less keypad). Because of the presence of that common line, the keypad generates ghost keys (either when permanently driving line 7, or when ignoring line 7 and scanning the matrix traditionally) when just two corners of a rectangle in the matrix are pressed (whereas a normal diode-less keypad without a common line generates ghost keys only when three corners of a rectangle are pressed).
Now that I know the configuration, scan_kp no longer needs to scan all eight lines; it suffices to scan just 0-2 and read 3-6 (or vice versa, since there are no diodes). But there's no harm in scanning all eight, other than some wasted time and power. */
void interpret_and_print_kp (void) {
	char key_array[]="123456789*0#"; //One byte for each key
	int i, j;
	clearscreen();
	for(i=3; i<7; i++) { //rows
		for(j=0; j<3; j++) { //columns
			if(kp[i]&(1<<j)) USARTWriteChar(' ');
			else USARTWriteChar(key_array[(i-3)*3+j]);
		}
		USARTWriteChar('\r');
		USARTWriteChar('\n');
	}
	/* uint16_t key_array=0; //12-bit array, one bit for each key
	int i, j;
	for(i=3; i<7; i++) { //rows
		for(j=0; j<3; j++) { //columns
			key_array|=kp[i]&(1<<j);
			key_array<<=1;
		}
	} */
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
		if(cmp_array(kp, new_kp)) { // Keypad state changed (either at user level, or due to bouncing; I'm not debouncing the keys).
			cpy_array(new_kp, kp);
			interpret_and_print_kp();
			//_delay_ms(10); //Debug; this alone fails to solve the glitches which the 1ms inter-line delay in scan_kp solves, but that delay alone solves the problem, so this 10ms delay is unnecessary.
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
