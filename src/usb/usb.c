//-----------------------------------------------------------------------------
// Project: CETI Tag Electronics
// Copyright: Harvard University Wood Lab
// Contributors: Michael Salino-Hugg, [TODO: Add other contributors here]
//-----------------------------------------------------------------------------
#include "main.h"
#include <stm32u5xx_hal.h>
#include <tusb.h>

void usb_init(void) {
  // init device stack on configured roothub port
  tusb_rhport_init_t dev_init = {.role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_HIGH};

  tusb_init(BOARD_TUD_RHPORT, &dev_init);
}

int usb_iface_present(void) {
  return (HAL_GPIO_ReadPin(IFACE_EN_GPIO_Input_GPIO_Port, IFACE_EN_GPIO_Input_Pin) == GPIO_PIN_SET);
}
