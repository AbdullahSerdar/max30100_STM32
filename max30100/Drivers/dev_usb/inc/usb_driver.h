/*
 * usb_driver.h
 *
 *  Created on: May 14, 2026
 *      Author: serda
 */

#ifndef DEV_USB_INC_USB_DRIVER_H_
#define DEV_USB_INC_USB_DRIVER_H_

#include <stdint.h>

typedef enum
{
    USB_DRV_OK = 0,
    USB_DRV_ERROR,
    USB_DRV_BUSY,
    USB_DRV_NOT_READY
} UsbDriver_Status_t;

typedef struct
{
    float bpm;
    float spo2;
    uint8_t spo2_valid;

    float ppg_ir;
    float ppg_red;

    uint16_t raw_ir;

    uint32_t timestamp_ms;
} UsbMax30100Packet_t;

void UsbDriver_Init(void);

UsbDriver_Status_t UsbDriver_PublishMax30100(const UsbMax30100Packet_t *packet);

UsbDriver_Status_t UsbDriver_WaitAndCopyMax30100(UsbMax30100Packet_t *packet,
                                                 uint32_t timeout_ms);

UsbDriver_Status_t UsbDriver_SendMax30100Packet(const UsbMax30100Packet_t *packet);

#endif /* DEV_USB_INC_USB_DRIVER_H_ */
