#include <avr/io.h>
#include <util/delay.h>
#include <inttypes.h>
#include <stdio.h>
#include <avr/interrupt.h>

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

#define strbuflen 256
#define T1_FREQ 15625 //16MHz clock divided by 2^10 prescaler
#define PRESS_BOUNCE_MS 5 //Max bounce time for keypress
#define RELEASE_BOUNCE_MS 20
#define SAMPLE_PERIOD_MS 1
unsigned int debounce_counter_init_press;
unsigned int debounce_counter_init_release;

unsigned char sbuf[strbuflen];

unsigned char kp_line; // Array of 8 bits, for keypad line state.
unsigned char kp[8]; // Array of 64 bits, for 8*8 matrix of keypad state (all lines).
unsigned char new_kp[8]; // New keypad state.
unsigned int presscounts[12]; //Count how many times each key is pressed.
unsigned int debouncecounters[12]; //Debounce countdown timer for each key.
int prevstates[12]; //Previous state (possibly still bouncing) of key. Would be type bool, but avr-gcc bitches about it.
int kpchanged; //Flag set (by debounce()) whenever kp changes.
unsigned int bouncecount; //Count of bounces (excluding genuine key state transitions).
unsigned int phantomcount; //Count of phantom keypresses (key settling back on previously debounced value after bouncing).

uint8_t krb[8]; // Keyboard report buffer

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
		asm("nop"); // http://support.atmel.no/knowledgebase/avrstudiohelp/mergedProjects/JTAGICEmkII/mkII/Html/JTAGICE_mkII_Special_Considerations.htm says "A NOP instruction must be placed between the OUT and the IN instruction to ensure that the correct value is present in the PIN register." But putting in just this nop doesn't solve the problem which the loop below does solve, indicating that the problem isn't originating in the uC itself, so the problem must be the breadboard wiring or the keypad itself.
		int foo;
		for(foo=0; foo<80; foo++) asm("nop"); //Without this (specifically, looping less than 8 times), the inter-line scanning runs fast enough that speed-intolerant breadboard wiring (or maybe it's the keypad itself) causes glitches, manifested by scan states not stabilizing when keys are pressed.
		//_delay_ms(1); //Had this until I discovered that the brief nop loop suffices.
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

void uartWriteArray(char* s, int l) {
	int i;
	for(i=0; i<l; i++) USARTWriteChar(s[i]);
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
	int i, j, k;
	clearscreen();
	for(i=3; i<7; i++) { //rows
		for(j=0; j<3; j++) { //columns
			k=(i-3)*3+j;
			sprintf(sbuf, "%c%c:%d\t", (kp[i]&(1<<j))?' ':'@', (key_array[k]), presscounts[k]);
			uartWriteString(sbuf);
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

char setbit(char c, int b, int v) {//c is char to modify, b is bit position, v is new value
	if(v) return c|(1<<b);
	else return c&~(1<<b);
}

void debounce_and_count (void) {
	int i, j, k;
	for(i=3; i<7; i++) { //rows
		for(j=0; j<3; j++) { //columns
			k=(i-3)*3+j;
			if(prevstates[k]!=(new_kp[i]&(1<<j))) { //Changed either due to genuine change, or due to bounce.
				prevstates[k]=(new_kp[i]&(1<<j));
				if(kp[i]&(1<<j)) debouncecounters[k]=debounce_counter_init_press;
				else debouncecounters[k]=debounce_counter_init_release;
				bouncecount++;
			} else {
				if(debouncecounters[k]!=0) {
					debouncecounters[k]--;
					if(debouncecounters[k]==0) {
						if(prevstates[k]!=(kp[i]&(1<<j))) {
							kp[i]=setbit(kp[i], j, prevstates[k]); //Key is stable, so update kp.
							if(!prevstates[k]) presscounts[k]++; //Active low.
							kpchanged=1;
							bouncecount--; //Exclude genuine transitions from bounce count.
						} else phantomcount++; //Key bounced but settled on previously debounced state, indicative either of an unsteadily-held key or EMI.
					}
				}
			}
		}
	}
}

void update_keypress_counts (void) {
	int i, j, k;
	for(i=3; i<7; i++) { //rows
		for(j=0; j<3; j++) { //columns
			k=(i-3)*3+j;
			if(!(kp[i]&(1<<j))) presscounts[k]++;
		}
	}
}

void fill_krb (void) {
	char scancodes[]={0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 4, 0x27, 5};
	int i, j, k, m;
	m=2;
	if(!(kp[6]&(1<<0))) krb[0]=2; //test; special case of asterisk to serve as shift
	else krb[0]=0;
	for(i=3; i<7; i++) { //rows
		for(j=0; j<3; j++) { //columns
			k=(i-3)*3+j;
			if(!(kp[i]&(1<<j)) && k!=9) { //Exclude key 9 (asterisk) since it's serving as shift
				krb[m]=scancodes[k];
				m++;
				if(m==8) return; //Max simultaneous USB HID keys reached
			}
		}
	}
	for(i=m; i<8; i++) krb[i]=0; //Zero out unused key slots
}

uint16_t volatile icount;

//Based on http://www.engblaze.com/we-interrupt-this-program-to-bring-you-a-tutorial-on-arduino-interrupts/
void interrupt_test_init() {
	icount=0;
	DDRD&=~4; PORTD|=4;
	EICRA |= (1 << ISC01);    // Trigger INT0 on falling edge
	EIMSK |= (1 << INT0);     // Enable external interrupt INT0
	// Oddly, even though internal pullup is enabled (confirmed by current draw when pin is grounded), if the pin is left externally disconnected, one single spurious interrupt is reliably generated as soon as INT0 is enabled. If the pin is externally pulled up, that spurious interrupt is reliably prevented.
	// In a separate test, connecting V+ and gnd through a SPDT to an RC network (time constant about 1s) to the interrupt pin (for both slow rise and slow fall), I usually (though not always) get multiple interrupts both during rise and during fall, proving that there's no schmitt trigger on the interrupt. Why isn't there one?
}

// Interrupt Service Routine attached to INT0 vector
ISR(INT0_vect) {
	icount++;
	clearscreen();
	sprintf(sbuf, "%d", icount);
	uartWriteString(sbuf);
}

ISR(TIMER1_OVF_vect) {
	sprintf(sbuf, " OVERFLOW");
	uartWriteString(sbuf);
}

ISR(TIMER1_COMPA_vect) {
	sprintf(sbuf, " CTCOVERFLOW");
	uartWriteString(sbuf);
}

int main (void)
{
	//DDRD|=0xfc; DDRB|=1;
	int i;
	USARTInit(0); //416 is 2400bps, 103 is 9600bps, 0 is 1Mbps. Can't do 115200bps reliably because UART clock is divided from the arduino uno's 16MHz master clock, which can't get near enough to 115.2kHz. See table 20-6 on page 200 of http://www.atmel.com/Images/doc8271.pdf for details.
	unsigned char c;
	//_delay_ms(2000); //debug
	//c=USARTReadChar();
	//sprintf(sbuf, "Starting...");
	uartWriteString(sbuf);
	//_delay_ms(1000); //debug
	//interrupt_test_init();
	//sei();
	OCR1A = 15; //Approx 1ms
	TCCR1B |= (1 << CS10); //|(1<<CS12); //|(1 << WGM12);
	//TIMSK1 = (1 << TOIE1);
	TIMSK1 = (1 << OCIE1A);
	uint16_t start, end, a1, a2, a3;
	debounce_counter_init_press=PRESS_BOUNCE_MS/SAMPLE_PERIOD_MS;
	if(PRESS_BOUNCE_MS%SAMPLE_PERIOD_MS) debounce_counter_init_press++;
	debounce_counter_init_release=RELEASE_BOUNCE_MS/SAMPLE_PERIOD_MS;
	if(RELEASE_BOUNCE_MS%SAMPLE_PERIOD_MS) debounce_counter_init_release++;

	//Initial pre-loop scan, in order to init data structures. Assume no keys are bouncing at this time.
	scan_kp();
	for(i=0; i<(debounce_counter_init_press>debounce_counter_init_release?debounce_counter_init_press:debounce_counter_init_release); i++) debounce_and_count();
	kpchanged=0;

	while(1) {
		scan_kp();
		debounce_and_count();
		_delay_ms(SAMPLE_PERIOD_MS);
		if(kpchanged) {
			kpchanged=0;
			fill_krb();
			uartWriteArray(krb, 8);
		}
	}
	while(1) {
		int k;
		for(k=0; k<9; k++) {
			krb[2]=k+4;
			uartWriteArray(krb, 8);
			krb[2]=0;
			uartWriteArray(krb, 8);
			_delay_ms(250);
		}
		while(1) _delay_ms(1000); //end
	}
	while(1) {
		//start/=16;
		//end/=16;
		TCNT1=0; scan_kp(); a1=TCNT1;
		TCNT1=0; debounce_and_count(); a2=TCNT1;
		_delay_ms(SAMPLE_PERIOD_MS); //Comment out for max scan speed, for maximum detection of bounces (rather than just filtering of bounces, which happens in any case).
		//if(cmp_array(kp, new_kp)) { // Keypad state changed (either at user level, or due to bouncing; I'm not debouncing the keys).
		if(kpchanged) { // Debounced keypad state changed.
			//Not needed since debounce() accomplishes this: cpy_array(new_kp, kp);
			kpchanged=0;
			//update_keypress_counts();
			TCNT1=0; interpret_and_print_kp(); a3=TCNT1;
			//_delay_ms(10); //Debug; this alone fails to solve the glitches which the 1ms inter-line delay in scan_kp solves, but that delay alone solves the problem, so this 10ms delay is unnecessary.
			TCNT1=0; end=TCNT1;
			sprintf(sbuf, "bouncecount:%d, phantomcount:%d, scan_kp=%d, debounce_and_count=%d, interpret_and_print_kp=%d", bouncecount, phantomcount, a1, a2, a3);
			uartWriteString(sbuf);
		}
	}

	while(1) {
		start=TCNT1;
		_delay_ms(1000);
		end=TCNT1;
		start/=16;
		end/=16;
		sprintf(sbuf, "%x.\r\n", end-start);
		uartWriteString(sbuf);
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
