/*****************************************************************************
 *   @file      usb/vendor.h
 *   @brief     USB vendor bulk interface for sensor data streaming
 *   @project   Project CETI
 *   @copyright Harvard University Wood Lab
 *   @authors   Michael Salino-Hugg
 *****************************************************************************/
#ifndef CETI_USB_VENDOR_H
#define CETI_USB_VENDOR_H

void usb_vendor_init(void);
void usb_vendor_task(void);

#endif // CETI_USB_VENDOR_H
