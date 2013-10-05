/* This code derived from the Keyboard demo in the LUFA stack.
   I include the LUFA copyright below.


   This file is supposed to abstract out the details of the USB
   layer from the rest of the firmware.

*/


/*
             LUFA Library
     Copyright (C) Dean Camera, 2013.

  dean [at] fourwalledcubicle [dot] com
           www.lufa-lib.org
*/

/*
  Copyright 2013  Dean Camera (dean [at] fourwalledcubicle [dot] com)

  Permission to use, copy, modify, distribute, and sell this
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaims all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

#include <LUFA/Drivers/USB/USB.h>
#include "kbdcont.h"
#include "Descriptors.h"

// Used by the HID layer
static uint8_t PrevKeyboardHIDReportBuffer[sizeof(USB_KeyboardReport_Data_t)];

// Hack to work around dropped reports on USB resume
static int should_repeat_report = 0;

USB_ClassInfo_HID_Device_t Keyboard_HID_Interface =
        {
                .Config =
                        {
                                .InterfaceNumber              = 0,
                                .ReportINEndpoint             =
                                        {
                                                .Address              = KEYBOARD_EPADDR,
                                                .Size                 = KEYBOARD_EPSIZE,
                                                .Banks                = 1,
                                        },
                                .PrevReportINBuffer           = PrevKeyboardHIDReportBuffer,
                                .PrevReportINBufferSize       = sizeof(PrevKeyboardHIDReportBuffer),
                        },
    };


uint8_t idle_rate;
uint8_t active_protocol;

void EVENT_USB_Device_Connect(void)
{
        SerialDebug(1, "UDevCon\r\n");
	debugLedOn(1);
}

/** Event handler for the library USB Disconnection event. */
void EVENT_USB_Device_Disconnect(void)
{
        SerialDebug(1, "UDevDisc\r\n");
	debugLedOff(1);
	debugLedOff(2);
}

/** Event handler for the library USB Configuration Changed event. */
void EVENT_USB_Device_ConfigurationChanged(void)
{
        bool ConfigSuccess = true;
        SerialDebug(1, "UDevConfChg\r\n");

        ConfigSuccess &= HID_Device_ConfigureEndpoints(&Keyboard_HID_Interface);

        USB_Device_EnableSOFEvents();

	debugLedOn(2);
}

/** Event handler for the library USB Control Request reception event. */
void EVENT_USB_Device_ControlRequest(void)
{
	SerialDebug(1, "UCTRL t %02x c %02x v %04x i %04x l %04x\r\n", USB_ControlRequest.bmRequestType,
		USB_ControlRequest.bRequest, USB_ControlRequest.wValue, USB_ControlRequest.wIndex, USB_ControlRequest.wLength);
        HID_Device_ProcessControlRequest(&Keyboard_HID_Interface);
}

void EVENT_USB_Device_StartOfFrame(void)
{
        HID_Device_MillisecondElapsed(&Keyboard_HID_Interface);
}

bool CALLBACK_HID_Device_CreateHIDReport(USB_ClassInfo_HID_Device_t* const HIDInterfaceInfo,
                                         uint8_t* const ReportID,
                                         const uint8_t ReportType,
                                         void* ReportData,
                                         uint16_t* const ReportSize)
{

	// Used when resending the last report to work around dropped reports on USB resume.
	static int resend = 0;

	SerialDebug(10, "CB_H_Dev_CrHIDRpt\r\n");

	if (resend) {
	  // Simply send the last report again
	  // Note - resend can never be true unless we have already sent at least one report, so copying
	  // the old report structure should be fine.
	  resend = 0;
	  memcpy(ReportData,PrevKeyboardHIDReportBuffer,sizeof(USB_KeyboardReport_Data_t));
	  *ReportSize = sizeof(USB_KeyboardReport_Data_t);
	  SerialDebug(1, "r*%02x%02x%02x%02x%02x%02x%02x%02x\r\n", ((uint8_t*)ReportData)[0],
	    ((uint8_t*)ReportData)[1], ((uint8_t*)ReportData)[2], ((uint8_t*)ReportData)[3],
	    ((uint8_t*)ReportData)[4], ((uint8_t*)ReportData)[5], ((uint8_t*)ReportData)[6], ((uint8_t*)ReportData)[7]);
	  return true;
	}
	
	if (do_suppress == 1) {
	  // don't return a report while in the supression window
	  unsigned long now = getMillis();
	  if ((signed long)now - (signed long)suppress_expire >= 0) {
	    do_suppress = 0;
          } else {
	    SerialDebug(2, "rptsup %lu exp %lu\r\n", now, suppress_expire);
	    *ReportSize = 0;
	    return false;
	  }
	}

	// Pull the next report off the queue
        void *report = dequeueReport();

	if (report == NULL) {
	  // Queue was empty
	  *ReportSize = 0;
          return false;
        } else {
          memcpy(ReportData, report, sizeof(USB_KeyboardReport_Data_t));
	  *ReportSize = sizeof(USB_KeyboardReport_Data_t);
	  SerialDebug(1, "r %02x%02x%02x%02x%02x%02x%02x%02x\r\n", ((uint8_t*)ReportData)[0],
	    ((uint8_t*)ReportData)[1], ((uint8_t*)ReportData)[2], ((uint8_t*)ReportData)[3],
	    ((uint8_t*)ReportData)[4], ((uint8_t*)ReportData)[5], ((uint8_t*)ReportData)[6], ((uint8_t*)ReportData)[7]);

	  // Did we just resume?
	  if (should_repeat_report) {
	    should_repeat_report = 0;
	    resend = 1;
	  }

	  return true;
	}
}

void CALLBACK_HID_Device_ProcessHIDReport(USB_ClassInfo_HID_Device_t* const HIDInterfaceInfo,
                                          const uint8_t ReportID,
                                          const uint8_t ReportType,
                                          const void* ReportData,
                                          const uint16_t ReportSize)
{
	int i;
	SerialDebug(1, "CB_H_Dev_PrcHIDRpt\r\n");
	SerialDebug(2, "Rpt data:");
	for (i=0;i<ReportSize;i++) {
	  SerialDebug(2, " %02x", ((uint8_t*)ReportData)[i]);
	}
	SerialDebug(2, "\r\n");
	kbdSetLeds(((uint8_t*)ReportData)[0]);
}

void USBCBSendResume(void)
{
  if (USB_Device_RemoteWakeupEnabled) {
	USB_Device_SendRemoteWakeup();
  } else {
    SerialDebug(1, "send res but !RemWakeEn\r\n");
    USB_Device_SendRemoteWakeup();
  }
}

void USB_Poll(void)
{
	HID_Device_USBTask(&Keyboard_HID_Interface);
	USB_USBTask();

}

uint8_t Get_USB_DeviceState(void)
{
  return USB_DeviceState;
}

void EVENT_USB_Device_Suspend(void)
{
  SerialDebug(1, "ususp UDINT = %02x, UDCON=%02x, UDIEN=%02x, device state = %d\r\n", UDINT, UDCON, UDIEN, USB_DeviceState);
  _delay_us(100);
  do_suspend = 1;
}

void EVENT_USB_Device_WakeUp(void)
{
  SerialDebug(1, "uwake UDINT = %02x, UDCON=%02x, UDIEN=%02x, device state = %d\r\n", UDINT, UDCON, UDIEN, USB_DeviceState);
  do_suspend = 0;
  pending_key_wake = 0;
  was_usb_wake = 1;
  should_repeat_report = 1;
}

void EVENT_USB_Device_Reset(void)
{
   SerialDebug(1, "DevRst\r\n");
}
