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
static uint8_t PrevExtraKeysHIDReportBuffer[sizeof(USB_ExtraKeysReport_Data_t)];

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
                                                .Size                 = HID_EPSIZE,
                                                .Banks                = 1,
                                        },
                                .PrevReportINBuffer           = PrevKeyboardHIDReportBuffer,
                                .PrevReportINBufferSize       = sizeof(PrevKeyboardHIDReportBuffer),
                        },
    };

USB_ClassInfo_HID_Device_t ExtraKeys_HID_Interface =
        {
                .Config =
                        {
                                .InterfaceNumber              = 1,
                                .ReportINEndpoint             =
                                        {
                                                .Address              = EXTRAKEYS_EPADDR,
                                                .Size                 = HID_EPSIZE,
                                                .Banks                = 1,
                                        },
                                .PrevReportINBuffer           = PrevExtraKeysHIDReportBuffer,
                                .PrevReportINBufferSize       = sizeof(PrevExtraKeysHIDReportBuffer),
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
        ConfigSuccess &= HID_Device_ConfigureEndpoints(&ExtraKeys_HID_Interface);

        USB_Device_EnableSOFEvents();

	debugLedOn(2);
}

/** Event handler for the library USB Control Request reception event. */
void EVENT_USB_Device_ControlRequest(void)
{
	// warning this one seems to cause initialization hangs if enabled
	SerialDebug(10, "UCTRL t %02x c %02x v %04x i %04x l %04x\r\n", USB_ControlRequest.bmRequestType,
		USB_ControlRequest.bRequest, USB_ControlRequest.wValue, USB_ControlRequest.wIndex, USB_ControlRequest.wLength);
        HID_Device_ProcessControlRequest(&Keyboard_HID_Interface);
        HID_Device_ProcessControlRequest(&ExtraKeys_HID_Interface);
}

void EVENT_USB_Device_StartOfFrame(void)
{
        HID_Device_MillisecondElapsed(&Keyboard_HID_Interface);
        HID_Device_MillisecondElapsed(&ExtraKeys_HID_Interface);
}

bool CALLBACK_HID_Device_CreateHIDReport(USB_ClassInfo_HID_Device_t* const HIDInterfaceInfo,
                                         uint8_t* const ReportID,
                                         const uint8_t ReportType,
                                         void* ReportData,
                                         uint16_t* const ReportSize)
{
	// Used when resending the last report to work around dropped reports on USB resume.
	// Will be set to 1 if it was the main interface, 2 for the secondary
	static int resend = 0;

	uint8_t qnum, *PrevBuffer;

	SerialDebug(10, "CB_H_Dev_CrHIDRpt q%d\r\n", qnum);

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

	/* Determine which interface must have its report generated */
	if (HIDInterfaceInfo == &Keyboard_HID_Interface)
        {
	  qnum = 1;
	  PrevBuffer = PrevKeyboardHIDReportBuffer;
	  *ReportSize = sizeof(USB_KeyboardReport_Data_t);
	} else {
	  qnum = 2;
	  PrevBuffer = PrevExtraKeysHIDReportBuffer;
	  *ReportSize = sizeof(USB_ExtraKeysReport_Data_t);
	}

	if (resend == qnum) {
	  // Simply send the last report again
	  // Note - resend can never be true unless we have already
	  //sent at least one report on that queue, so copying
	  // the old report structure should be fine.
	  resend = 0;
	  memcpy(ReportData,PrevBuffer,*ReportSize);
	} else {
	  // Pull the next report off the queue
	  void *report = dequeueReport(qnum);

	  if (report == NULL) {
	    // Queue was empty
	    *ReportSize = 0;
	    return false;
	  } else {
	    memcpy(ReportData, report, *ReportSize);
	  }
	}
	
#if SERIALDEBUG >= 1
	{
	  int i;
	  SerialDebug2("r*");
	  for (i=0; i<(*ReportSize);i++) { SerialDebug2("%02x", ((uint8_t*)ReportData)[i]); }
	  SerialDebug2("\r\n");
	}
#endif
	// Did we just resume?
	if (should_repeat_report) {
	  should_repeat_report = 0;
	  resend = qnum;
	}

	return true;
}

void CALLBACK_HID_Device_ProcessHIDReport(USB_ClassInfo_HID_Device_t* const HIDInterfaceInfo,
                                          const uint8_t ReportID,
                                          const uint8_t ReportType,
                                          const void* ReportData,
                                          const uint16_t ReportSize)
{
	int i;
	SerialDebug(2, "CB_H_Dev_PrcHIDRpt\r\n");
	SerialDebug(2, "Rpt data:");
	for (i=0;i<ReportSize;i++) {
	  SerialDebug(2, " %02x", ((uint8_t*)ReportData)[i]);
	}
	SerialDebug(2, "\r\n");
	if (HIDInterfaceInfo == &Keyboard_HID_Interface)
        {
	  kbdSetLeds(((uint8_t*)ReportData)[0]);
	} else {
	  kbdSetExtraLeds(((uint8_t*)ReportData)[0]);
	}
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
	HID_Device_USBTask(&ExtraKeys_HID_Interface);
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
  // This is to work around a strange state we _very_ occasionally hit where
  // we get here but the host doesn't seem to think we should be awake and
  // isn't sending us SOF events. This is bad, because we won't poke the host
  // to wake us up, since we think it already has. Work around it by
  // pretending it was a keypress wakeup, in which case we will go back to
  // sleep when the code finds there isn't _really_ a live key.
  if (((UDINT & 0x10) == 0) && (UDIEN & 0x04)) {
    SerialDebug(1, "FAKE USB WAKE\r\n");
    was_usb_wake = 0;
    should_repeat_report = 0;
  } else {
    was_usb_wake = 1;
    should_repeat_report = 1;
  }

}

void EVENT_USB_Device_Reset(void)
{
   SerialDebug(1, "DevRst\r\n");
}
