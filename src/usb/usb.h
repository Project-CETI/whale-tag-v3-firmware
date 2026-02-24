//-----------------------------------------------------------------------------
// Project: CETI Tag Electronics
// Copyright: Harvard University Wood Lab
// Contributors: Michael Salino-Hugg, [TODO: Add other contributors here]
//-----------------------------------------------------------------------------
#ifndef CETI_WHALE_TAG_USB_H
#define CETI_WHALE_TAG_USB_H

void usb_system_init(void);
void usb_init(void);

int usb_iface_present(void);

#endif // CETI_WHALE_TAG_USB_H