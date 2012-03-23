/*
 See the file License in this directory for the license for this file.
 This program is derived from http://hunt.net.nz/users/darran/weblog/b3029/attachments/bd341/arduino-keyboard-0.3.tar.gz which in turn is derived from a demo program in Dean Camera's LUFA Library.
 Darran's revision history:
 * Date         Rev  Description
 * 21-Mar-2011  0.1  Initial version.
 * 23-Mar-2011  0.2  Improved handling of serial reports to ensure that all reports
 *                   will be sent.
 * 13-Apr-2011  0.3  Extended range of keys from 101 to 231.
 */

 //Modified by me to use 1Mbps instead of 9600bps, and to use fletcher16 checksum and to flush input garbage.

/** \file
 *
 *  Main source file for the Arduino-keyboard project. This file contains the main tasks of
 *  the project and is responsible for the initial application hardware configuration.
 */

#include "Arduino-keyboard.h"

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

/** Buffer to hold the previously generated Keyboard HID report, for comparison purposes inside the HID class driver. */
uint8_t PrevKeyboardHIDReportBuffer[sizeof(USB_KeyboardReport_Data_t)];
uint8_t uartRcvBuf[sizeof(USB_KeyboardReport_Data_t)+2]; //Includes fletcher16 checksum

/** LUFA HID Class driver interface configuration and state information. This structure is
 *  passed to all HID Class driver functions, so that multiple instances of the same class
 *  within a device can be differentiated from one another.
 */
USB_ClassInfo_HID_Device_t Keyboard_HID_Interface =
 	{
		.Config =
			{
				.InterfaceNumber              = 0,

				.ReportINEndpointNumber       = KEYBOARD_EPNUM,
				.ReportINEndpointSize         = KEYBOARD_EPSIZE,
				.ReportINEndpointDoubleBank   = false,

				.PrevReportINBuffer           = PrevKeyboardHIDReportBuffer,
				.PrevReportINBufferSize       = sizeof(PrevKeyboardHIDReportBuffer),
			},
    };

/** Main program entry point. This routine contains the overall program flow, including initial
 *  setup of all components and the main program loop.
 */

/** Circular buffer to hold data from the serial port before it is sent to the host. */
RingBuff_t USARTtoUSB_Buffer;

uint8_t keyboardData[8] = { 0 };
uint8_t ledReport = 0;

/** Main program entry point. This routine contains the overall program flow, including initial
 *  setup of all components and the main program loop.
 */
int main(void)
{
	SetupHardware();

	RingBuffer_InitBuffer(&USARTtoUSB_Buffer);

	sei();

	for (;;)
	{
		HID_Device_USBTask(&Keyboard_HID_Interface);
		USB_USBTask();
	}
}

/** Configures the board hardware and chip peripherals for the demo's functionality. */
void SetupHardware(void)
{
	/* Disable watchdog if enabled by bootloader/fuses */
	MCUSR &= ~(1 << WDRF);
	wdt_disable();

	/* Hardware Initialization */
	Serial_Init(1000000, false); UCSR1B |= 1 << RXCIE1;
	USB_Init();

	/* Start the flush timer so that overflows occur rapidly to push received bytes to the USB interface */
	TCCR0B = (1 << CS02);
	
	/* Pull target /RESET line high */
	AVR_RESET_LINE_PORT |= AVR_RESET_LINE_MASK;
	AVR_RESET_LINE_DDR  |= AVR_RESET_LINE_MASK;
}

/** Event handler for the library USB Connection event. */
void EVENT_USB_Device_Connect(void)
{
}

/** Event handler for the library USB Disconnection event. */
void EVENT_USB_Device_Disconnect(void)
{
}

/** Event handler for the library USB Configuration Changed event. */
void EVENT_USB_Device_ConfigurationChanged(void)
{
	HID_Device_ConfigureEndpoints(&Keyboard_HID_Interface);
	USB_Device_EnableSOFEvents();
}

/** Event handler for the library USB Unhandled Control Request event. */
void EVENT_USB_Device_UnhandledControlRequest(void)
{
	HID_Device_ProcessControlRequest(&Keyboard_HID_Interface);
}

/** Event handler for the USB device Start Of Frame event. */
void EVENT_USB_Device_StartOfFrame(void)
{
	HID_Device_MillisecondElapsed(&Keyboard_HID_Interface);
}

/** HID class driver callback function for the creation of HID reports to the host.
 *
 *  \param[in]     HIDInterfaceInfo  Pointer to the HID class interface configuration structure being referenced
 *  \param[in,out] ReportID    Report ID requested by the host if non-zero, otherwise callback should set to the generated report ID
 *  \param[in]     ReportType  Type of the report to create, either REPORT_ITEM_TYPE_In or REPORT_ITEM_TYPE_Feature
 *  \param[out]    ReportData  Pointer to a buffer where the created report should be stored
 *  \param[out]    ReportSize  Number of bytes written in the report (or zero if no report is to be sent
 *
 *  \return Boolean true to force the sending of the report, false to let the library determine if it needs to be sent
 */
bool CALLBACK_HID_Device_CreateHIDReport(
    USB_ClassInfo_HID_Device_t* const HIDInterfaceInfo,
    uint8_t* const ReportID,
    const uint8_t ReportType,
    void* ReportData,
    uint16_t* const ReportSize)
{
	uint8_t *datap = ReportData;
	int ind;

	RingBuff_Count_t BufferCount = RingBuffer_GetCount(&USARTtoUSB_Buffer);

	uint8_t checkA, checkB;
	if (BufferCount >= 10) {
		for (ind=0; ind<10; ind++) {
			uartRcvBuf[ind] = RingBuffer_Remove(&USARTtoUSB_Buffer);
		}
		fletcher16(&checkA, &checkB, uartRcvBuf, 8);
		if (checkA==uartRcvBuf[8] && checkB==uartRcvBuf[9]) {
		    for (ind=0; ind<8; ind++) {
			keyboardData[ind] = uartRcvBuf[ind];
		    }
		    /* Send an led status byte back for every keyboard report received */
		    Serial_TxByte(ledReport);
		} else while(RingBuffer_GetCount(&USARTtoUSB_Buffer)) RingBuffer_Remove(&USARTtoUSB_Buffer); //Bad checksum; assume possibly other garbage in ring buffer which breaks the 10-byte alignment, so flush the buffer.
	}

	for (ind=0; ind<8; ind++) {
	    datap[ind] = keyboardData[ind];
	}

	*ReportSize = sizeof(USB_KeyboardReport_Data_t);
	return false;
}

/** HID class driver callback function for the processing of HID reports from the host.
 *
 *  \param[in] HIDInterfaceInfo  Pointer to the HID class interface configuration structure being referenced
 *  \param[in] ReportID    Report ID of the received report from the host
 *  \param[in] ReportType  The type of report that the host has sent, either REPORT_ITEM_TYPE_Out or REPORT_ITEM_TYPE_Feature
 *  \param[in] ReportData  Pointer to a buffer where the created report has been stored
 *  \param[in] ReportSize  Size in bytes of the received HID report
 */
void CALLBACK_HID_Device_ProcessHIDReport(USB_ClassInfo_HID_Device_t* const HIDInterfaceInfo,
                                          const uint8_t ReportID,
                                          const uint8_t ReportType,
                                          const void* ReportData,
                                          const uint16_t ReportSize)
{
    /* Need to send status back to the Arduino to manage caps, scrolllock, numlock leds */
   ledReport = *((uint8_t *)ReportData);
}

/** ISR to manage the reception of data from the serial port, placing received bytes into a circular buffer
 *  for later transmission to the host.
 */
ISR(USART1_RX_vect, ISR_BLOCK)
{
	uint8_t ReceivedByte = UDR1;

	if (USB_DeviceState == DEVICE_STATE_Configured)
	  RingBuffer_Insert(&USARTtoUSB_Buffer, ReceivedByte);
}
