//-----------------------------------------------------------------------------
// Project: CETI Tag Electronics
// Copyright: Harvard University Wood Lab
// Contributors: Michael Salino-Hugg
//-----------------------------------------------------------------------------
#include "main.h"
#include <stm32u5xx_hal.h>
#include <tusb.h>

#include "cdc.h"
#include "vendor.h"

// Configure only USB-specific clocks on top of existing SystemClock_Config.
static void __usb_clock_config(void) {
    __HAL_RCC_SYSCFG_CLK_ENABLE();

    // Route USB PHY clock from HSE
    RCC_PeriphCLKInitTypeDef usb_clk_init = {0};
    usb_clk_init.PeriphClockSelection = RCC_PERIPHCLK_USBPHY;
    usb_clk_init.UsbPhyClockSelection = RCC_USBPHYCLKSOURCE_HSE;
    if (HAL_RCCEx_PeriphCLKConfig(&usb_clk_init) != HAL_OK) {
        Error_Handler();
    }

    HAL_SYSCFG_SetOTGPHYReferenceClockSelection(SYSCFG_OTG_HS_PHY_CLK_SELECT_1);
}

void usb_system_init(void) {
    // USB PHY clock (adds to existing SystemClock_Config)
    __usb_clock_config();

    /* Configure DM DP Pins (PA11, PA12) */
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.Pin = (GPIO_PIN_11 | GPIO_PIN_12);
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF10_USB;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* USB OTG HS + PHY clocks */
    __HAL_RCC_USB_OTG_HS_CLK_ENABLE();
    __HAL_RCC_USBPHYC_CLK_ENABLE();

    /* Enable USB power */
    HAL_PWREx_EnableVddUSB();
    HAL_PWREx_EnableUSBHSTranceiverSupply();

    /* Enable OTG HS PHY */
    HAL_SYSCFG_EnableOTGPHY(SYSCFG_OTG_HS_PHY_ENABLE);

    // Disable VBUS sense (B device)
    USB_OTG_HS->GCCFG &= ~USB_OTG_GCCFG_VBDEN;

    // B-peripheral session valid override enable
    USB_OTG_HS->GCCFG |= USB_OTG_GCCFG_VBVALEXTOEN;
    USB_OTG_HS->GCCFG |= USB_OTG_GCCFG_VBVALOVAL;

    // Force device mode
    USB_OTG_HS->GUSBCFG |= USB_OTG_GUSBCFG_FDMOD;

    /* USB interrupt */
    HAL_NVIC_SetPriority(OTG_HS_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(OTG_HS_IRQn);
}

void usb_init(void) {
    // init device stack on configured roothub port
    tusb_rhport_init_t dev_init = {.role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_HIGH};

    /* initialize USB hardware */
    usb_system_init();
    HAL_Delay(100);

    tusb_init(BOARD_TUD_RHPORT, &dev_init);
    usb_cdc_init();
    usb_vendor_init();
}

void usb_task(void) {
    tud_task();
    usb_cdc_task();
    usb_vendor_task();
}

int usb_iface_present(void) {
    return (HAL_GPIO_ReadPin(IFACE_EN_GPIO_Input_GPIO_Port, IFACE_EN_GPIO_Input_Pin) == GPIO_PIN_SET);
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void) {
    //  blink_interval_ms = BLINK_MOUNTED;
    //	 led_blink(LED_YELLOW);
    //	 CETI_LOG("USB mounted");
}

// Invoked when device is unmounted
void tud_umount_cb(void) {
    //	 led_on(LED_YELLOW);
    //	 CETI_LOG("USB unmounted");
    //  blink_interval_ms = BLINK_NOT_MOUNTED;
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en) {
    //  (void) remote_wakeup_en;
    //  blink_interval_ms = BLINK_SUSPENDED;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void) {
    //  blink_interval_ms = tud_mounted() ? BLINK_MOUNTED : BLINK_NOT_MOUNTED;
}
