#include <avr/io.h>
#include <util/delay.h>
#include <inttypes.h>
#include <stdio.h>
#include <avr/interrupt.h>

#include "keydefs.h"

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

#define USE_QWERTY_FAKES 1 //Send scancodes according to standard qwerty, using fake keys and fake presses of shift, so that the mapping in the OS can be standard qwerty, and brain-dead VMMs such as VMware which provide only PS/2 virtual keyboards to VMs will pass all the keys (dlyz natively uses USB HID page 7 codes which have no PS/2 counterparts, so VMware drops those keys when sent to VMs). For example, when colon is pressed, instead of sending dlyz native 0xcb (which _is_ the standard code for colon, though X on Debian 6 doesn't recognize it), send shift-semicolon.
#define MAX_ROLLOVER 16

unsigned int debounce_counter_init_press;
unsigned int debounce_counter_init_release;

uint8_t sbuf[strbuflen];

uint16_t kb[7]; // Array of 7 rows of 10 columns. Using uint16_t wastes 6 bits, but simplifies the program, and the waste doesn't matter since the microcontroller is dedicated to keyboard scanning and has resources to spare.
uint16_t new_kb[7]; // New keypad state.
unsigned int presscounts[70]; //Count how many times each key is pressed.
unsigned int debouncecounters[70]; //Debounce countdown timer for each key. Wasteful to use a full byte since I'm only using a count of 4, but this simplifies the program.
int prevstates[70]; //Previous state (possibly still bouncing) of key. Would be type bool, but avr-gcc bitches about it.
int kbchanged; //Flag set (by debounce()) whenever kb changes.
int fnchanged; //Flag set (by debounce()) whenever a fn key changes.
unsigned int bouncecount; //Count of bounces (excluding genuine key state transitions).
unsigned int phantomcount; //Count of phantom keypresses (key settling back on previously debounced value after bouncing).

typedef struct {
	uint8_t mod_keys;
	uint8_t reserved;
	uint8_t nonmod_keys[6];
	uint8_t fletcher16_checksum[2];
} krb_t;
krb_t krb_v;

uint8_t mod_keys;
uint8_t rollover_buffer[MAX_ROLLOVER]; //All pressed non-modifier keys, ordered by time of press.
int i_rollover_next; //Index into rollover_buffer of next free slot. Zero if no keys in the buffer (which means no non-modifier key is pressed).
uint8_t lastkey; //The most recently pressed non-modifier key.
int lastkey_stillpressed; //False either if no non-modifier keys pressed, or if last pressed non-modifier key is released while previously-pressed non-modifier keys are still held.

int fn_pressed; //Would be type bool, but avr-gcc bitches about it.
int l_fn_pressed; //Left fn key.
int r_fn_pressed;

//Copied from http://en.wikipedia.org/wiki/Fletcher%27s_checksum
void fletcher16( uint8_t *checkA, uint8_t *checkB, uint8_t *data, size_t len )
{
        uint16_t sum1 = 0xff, sum2 = 0xff;

        while (len) {
                size_t tlen = len > 21 ? 21 : len;
                len -= tlen;
                do {
                        sum1 += *data++;
                        sum2 += sum1;
                } while (--tlen);
                sum1 = (sum1 & 0xff) + (sum1 >> 8);
                sum2 = (sum2 & 0xff) + (sum2 >> 8);
        }
        /* Second reduction step to reduce sums to 8 bits */
        sum1 = (sum1 & 0xff) + (sum1 >> 8);
        sum2 = (sum2 & 0xff) + (sum2 >> 8);
        *checkA = (uint8_t)sum1;
        *checkB = (uint8_t)sum2;
}

uint16_t read_kb_line() { // Return new keypad column state.
	uint16_t x;
	x=PINB&0x1e; x<<=5; //Pins 1-4 of port B (Arduino pins digital 9 through digital 12) are columns 6-9.
	x|=PINC&0x3f; //Pins 0-5 of port C (Arduino pins A0-A5) are columns 0-5.
	return x;
}

void set_line(int i) { //Set the selected line to output and drive it low, and set all other lines to input and enable pullup resistors.
	int lineB=0, lineD=0;
		if(i<6) lineD=1<<(i+2); //Pins 2-7 of port D (Arduino pins digital 2 through digital 7) are rows 0-6.
		else lineB=1; //Pin 0 of port B (Arduino pin digital 8) is row 7.
		DDRD&=3; //Set all rows to input, but leave pins 0 and 1 alone since Arduino uses the UART.
		DDRB&=0xfe;
		PORTD=(PORTD&3)|((~lineD)&0xfc); // Set the selected line to drive low when set to output, and enable pullup resistors on all the input lines so they don't float (though floating doesn't matter).
		PORTB=(PORTB&0xfe)|((~lineB)&1);
		DDRD|=lineD; // Set the selected line to output. Since the selected line was previously an input, with the internal pullup resistor enabled, and the previously selected line was driven low, and AVR when switching an interally-pulled-up input to an output causes it to drive high, setting all lines as inputs before setting a new output avoids the risk of shorting a driven-high pin and a driven-low pin during the transition period if the keypad is wired wrong.
		DDRB|=lineB;
}

void scan_kb() {
	int i, delay;
	for(i=0; i<7; i++) { // Drive each row, one at a time, and read all columns after each drive.
		set_line(i);
		for(delay=0; delay<16; delay++) asm("nop"); //Without this (specifically, looping less than 8 times), the inter-line scanning runs fast enough that speed-intolerant breadboard wiring (or maybe it's the keypad itself) causes glitches, manifested by scan states not stabilizing when keys are pressed. Haven't tried yet with full keyboard, just with the 12-key keypad.
		new_kb[i]=read_kb_line();
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

uint8_t USARTReadChar() {
	while(!(UCSRA & (1<<RXC))); //Wait until a data is available
	return UDR; //Now USART has got data from host and is available is buffer
}

void USARTWriteChar(uint8_t data) {
	while(!(UCSRA & (1<<UDRE))); //Wait until the transmitter is ready
	UDR=data; //Now write the data to USART buffer
}

void uartWriteString(uint8_t* s) {
	int i;
	for(i=0; i<strbuflen; i++) {
		if(s[i]=='\0') break;
		USARTWriteChar(s[i]);
	}
}

void uartWriteArray(uint8_t* s, int l) {
	int i;
	for(i=0; i<l; i++) USARTWriteChar(s[i]);
}

uint16_t setbit(uint16_t c, int b, int v) {//c is bitfield to modify, b is bit position, v is new value
	if(v) return c|(1<<b);
	else return c&~(1<<b);
}

int istrue(uint16_t x) { //C sucks.
	if(x) return 1;
	return 0;
}

void delete_key(int r) { //Delete the key and compact the rollover buffer.
	int i;
	i_rollover_next--;
	if(r==i_rollover_next) lastkey_stillpressed=0;
	for(i=r; i<i_rollover_next; i++) rollover_buffer[i]=rollover_buffer[i+1];
	rollover_buffer[i]=0;
}

void update_key(int i, int j, int k) {
	if(prevstates[k]!=istrue(kb[i]&(1<<j))) { //Key changed and has finished bouncing.
		int i_logical_key; //Index into logical_key_matrix or logical_fn_matrix of key.
		int logical_key;
		uint8_t modkey=0;
		int fnkey; //Would be type bool, but avr-gcc bitches about it.
		fnkey=0;
		kb[i]=setbit(kb[i], j, prevstates[k]); //Key is stable, so update kb.
		i_logical_key=convert_electronic_matrix_to_logical(i, j);
		logical_key=fn_pressed?logical_fn_matrix[i_logical_key]:logical_key_matrix[i_logical_key];
		switch(logical_key_matrix[i_logical_key]) {
			case Kl_ctrl: modkey=MKl_ctrl; break;
			case Kl_shft: modkey=MKl_shft; break;
			case Kl_alt: modkey=MKl_alt; break;
			case Kl_win: modkey=MKl_win; break;
			case Kr_ctrl: modkey=MKr_ctrl; break;
			case Kr_shft: modkey=MKr_shft; break;
			case Kr_altgr: modkey=MKr_altgr; break;
			case Kr_win: modkey=MKr_win; break;
			case Kl_fn: fnkey=1; break;
			case Kr_fn: fnkey=1; break;
		}
		if(fnkey) {
			if(!prevstates[k]) { //Active low; key pressed.
				presscounts[k]++;
				if(logical_key==Kl_fn) l_fn_pressed=1;
				else r_fn_pressed=1;
				fn_pressed=1;
			} else {
				if(logical_key==Kl_fn) l_fn_pressed=0;
				else r_fn_pressed=0;
				fn_pressed=l_fn_pressed|r_fn_pressed;
			}
			fnchanged=1;
		} else if(modkey) {
			if(!prevstates[k]) { //Key pressed.
				presscounts[k]++;
				mod_keys|=modkey;
			} else mod_keys&=~modkey; //Key released.
			kbchanged=1;
		} else { //Non-mod key.
			if(!prevstates[k]) { //Key pressed.
				presscounts[k]++;
				lastkey=logical_key;
				lastkey_stillpressed=1;
				if(i_rollover_next!=MAX_ROLLOVER) {
					rollover_buffer[i_rollover_next]=logical_key;
					i_rollover_next++;
				}
			} else { //Key released.
				int r;
				for(r=0; r<i_rollover_next; r++) {
					if((rollover_buffer[r]==logical_fn_matrix[i_logical_key])||(rollover_buffer[r]==logical_key_matrix[i_logical_key])) { //Nonmod key could be released after fn is released, in which case logical_key here will be from logical_key_matrix but the entry in rollover_buffer will be from logical_fn_matrix, so it's necessary to check for either case.
						delete_key(r);
						break;
					}
				}
			}
			kbchanged=1;
		}
		bouncecount--; //Exclude genuine transitions from bounce count.
	} else phantomcount++; //Key bounced but settled on previously debounced state, indicative either of an unsteadily-held key or EMI.
}

void debounce_and_count (void) {
	int i, j, k;
	for(i=0; i<7; i++) { //rows
		for(j=0; j<10; j++) { //columns
			k=i*10+j;
			if(prevstates[k]!=istrue(new_kb[i]&(1<<j))) { //Changed either due to genuine change, or due to bounce.
				prevstates[k]=istrue(new_kb[i]&(1<<j));
				if(kb[i]&(1<<j)) debouncecounters[k]=debounce_counter_init_press;
				else debouncecounters[k]=debounce_counter_init_release;
				bouncecount++;
			} else { //Key unchanged relative to last scan.
				if(debouncecounters[k]!=0) {
					debouncecounters[k]--;
					if(debouncecounters[k]==0) update_key(i, j, k);
				}
			}
		}
	}
}

int convert_electronic_matrix_to_logical(unsigned int row, unsigned int column) { //Convert 10 column by 7 row to 12 by 6, with rows 0-5 of the source mapping directly to the target, and row 6 mapping to columns 0xa and 0xb of the target. This enables the matrix to be scanned with just 17 lines, rather than the 18 which a 12 by 6 electronic matrix would require.
        if(row!=6) return row*12+column+1;
        if(column<5) return column*12;
        return (9-column)*12+0xb;
}

/* Obsolete function.
void fill_krb_from_scratch (void) {
	int i, j, n;
	n=0; //Index into krb_v.nonmod_keys of next unused slot.
	krb_v.mod_keys=0;
	int i_logical_key;
	for(i=0; i<7; i++) { //rows
		for(j=0; j<10; j++) { //columns
			i_logical_key=convert_electronic_matrix_to_logical(i, j);
			if(!(kb[i]&(1<<j))) {
				uint8_t modkey=0;
				int fnkey; //Would be type bool, but avr-gcc bitches about it.
				fnkey=0;
				switch(logical_key_matrix[i_logical_key]) {
					case Kl_ctrl: modkey=MKl_ctrl; break;
					case Kl_shft: modkey=MKl_shft; break;
					case Kl_alt: modkey=MKl_alt; break;
					case Kl_win: modkey=MKl_win; break;
					case Kr_ctrl: modkey=MKr_ctrl; break;
					case Kr_shft: modkey=MKr_shft; break;
					case Kr_altgr: modkey=MKr_altgr; break;
					case Kr_win: modkey=MKr_win; break;
					case Kl_fn: fnkey=1; break;
					case Kr_fn: fnkey=1; break;
				}
				if(modkey) krb_v.mod_keys|=modkey;
				else if(!fnkey) { //Don't send fn key code; keyboard alone handles it.
					if(fn_pressed) krb_v.nonmod_keys[n]=logical_fn_matrix[i_logical_key];
					else krb_v.nonmod_keys[n]=logical_key_matrix[i_logical_key];
					n++;
					if(n==6) return; //Max simultaneous USB HID keys reached
				}
			}
		}
	}
	for(i=n; i<6; i++) krb_v.nonmod_keys[i]=0; //Zero out unused key slots
}
*/

void fill_krb (void) {
	int i;
	krb_v.mod_keys=mod_keys;
	for(i=0; i<6; i++)  krb_v.nonmod_keys[i]=rollover_buffer[i];
}

void fill_qwerty_fake_krb(void) {
	int i;
	krb_v.mod_keys=mod_keys;
	for(i=0; i<6; i++)  krb_v.nonmod_keys[i]=0;
	if(!lastkey_stillpressed) return;
	krb_v.nonmod_keys[0]=lastkey;
	if(mod_keys&(MKl_ctrl|MKl_alt|MKl_win|MKr_ctrl|MKr_altgr|MKr_win)) return;
	int real_shift, qwerty_fake_shift, qwerty_fake_key;
	real_shift=mod_keys&(MKl_shft|MKr_shft);
	qwerty_fake_shift=real_shift;
	qwerty_fake_key=lastkey;
	switch(lastkey) {
		case K2:
		case K3:
		case K4:
		case K6:
		case K7:
		case K8:
		case K9:
		case Kequal:
		case Kleftbracket:
		case Krightbracket:
		case Kasterisk:
		case Kplus:
			if(real_shift) {qwerty_fake_shift=0; qwerty_fake_key=Kunsupported;} break;
		case K0:
			if(real_shift) qwerty_fake_key=K5; break;
		case K1:
			if(real_shift) qwerty_fake_key=K3; break;
		case K5:
			if(real_shift) qwerty_fake_key=K6; break;
		case Kunderline:
			if(real_shift) {qwerty_fake_shift=0; qwerty_fake_key=Kbacktick;}
			else {qwerty_fake_shift=MKl_shft; qwerty_fake_key=Kdash;}
			break;
		case Kdash:
			if(real_shift) qwerty_fake_key=Kbacktick; break;
		case Kslash:
			if(real_shift) qwerty_fake_key=K2; break;
		case Kperiod:
			if(real_shift) qwerty_fake_key=K1; break;
		case Kcomma:
			if(real_shift) qwerty_fake_key=K4; break;
		case Kleft_parenthesis:
			if(real_shift) qwerty_fake_key=Kleftbracket;
			else {qwerty_fake_shift=MKl_shft; qwerty_fake_key=K9;}
			break;
		case Kright_parenthesis:
			if(real_shift) qwerty_fake_key=Krightbracket;
			else {qwerty_fake_shift=MKl_shft; qwerty_fake_key=K0;}
			break;
		case Kquestionmark:
			if(real_shift) qwerty_fake_key=Kcomma;
			else {qwerty_fake_shift=MKl_shft; qwerty_fake_key=Kslash;}
			break;
		case Ksemicolon:
			if(real_shift) qwerty_fake_key=K7; break;
		case Kcolon:
			if(real_shift) qwerty_fake_key=Kbackslash;
			else {qwerty_fake_shift=MKl_shft; qwerty_fake_key=Ksemicolon;}
			break;
		case Kbackslash:
			if(real_shift) qwerty_fake_key=Kperiod; break;
	}
	krb_v.mod_keys=(mod_keys&~(MKl_shft|MKr_shft))|qwerty_fake_shift;
	krb_v.nonmod_keys[0]=qwerty_fake_key;
}

void clearscreen(void) {
                        sprintf(sbuf, "%c[2J\r", 0x1b);
                        uartWriteString(sbuf);
}

void print_rollover_buffer(void) {
	int i;
	for(i=0; i<16; i++) {
		sprintf(sbuf, "%x ", rollover_buffer[i]);
		uartWriteString(sbuf);
	}
	sprintf(sbuf, "\r\nlastkey:%x, lastkey_stillpressed:%x\r\n", lastkey, lastkey_stillpressed);
	uartWriteString(sbuf);
}

void print_kb (void) {
        int i; int j;
        clearscreen();
        for(i=0; i<7; i++) {
                for(j=0; j<10; j++) {
                        //if(i==j) sbuf[j]='_'; // Driven pin is always low; mark as '_' instead of '0' for clarity.
                        if((kb[i])&(1<<j)) sbuf[j]='1';
                        else sbuf[j]='0';
                }
                sprintf(sbuf+10, "\r\n");
                uartWriteString(sbuf);
        }
}

int main (void)
{
	int i;
	DDRB&=~0x1e; DDRC&=~0x3f; //Set columns to input
	PORTB|=0x1e; PORTC|=0x3f; //Enable pullup resistors
	USARTInit(0); //416 is 2400bps, 103 is 9600bps, 0 is 1Mbps. Can't do 115200bps reliably because UART clock is divided from the arduino uno's 16MHz master clock, which can't get near enough to 115.2kHz. See table 20-6 on page 200 of http://www.atmel.com/Images/doc8271.pdf for details.
	debounce_counter_init_press=PRESS_BOUNCE_MS/SAMPLE_PERIOD_MS;
	if(PRESS_BOUNCE_MS%SAMPLE_PERIOD_MS) debounce_counter_init_press++;
	debounce_counter_init_release=RELEASE_BOUNCE_MS/SAMPLE_PERIOD_MS;
	if(RELEASE_BOUNCE_MS%SAMPLE_PERIOD_MS) debounce_counter_init_release++;

	//Initial pre-loop scan, in order to init data structures. Assume no keys are bouncing at this time.
//	scan_kb();
//	for(i=0; i<(debounce_counter_init_press>debounce_counter_init_release?debounce_counter_init_press+1:debounce_counter_init_release+1); i++) debounce_and_count();

	for(i=0; i<7; i++) {
		kb[i]=0x3ff; //Initialize 10 columns to inactive. Active low.
		new_kb[i]=0x3ff;
	}
	for(i=0; i<70; i++) prevstates[i]=1; //Initialize all keys to inactive. Active low.
	kbchanged=0;
	fnchanged=0;

	while(1) {
		scan_kb();
		debounce_and_count();
		_delay_ms(SAMPLE_PERIOD_MS);
		if(kbchanged) {
			kbchanged=0;
			if(USE_QWERTY_FAKES) fill_qwerty_fake_krb();
			else fill_krb();
			fletcher16(krb_v.fletcher16_checksum, krb_v.fletcher16_checksum+1, (uint8_t*)&krb_v, sizeof(krb_t)-2);
			uartWriteArray((uint8_t*)&krb_v, sizeof(krb_t));
//			print_rollover_buffer(); //debug
//			print_kb();
		}
	}
	return 0;
}

