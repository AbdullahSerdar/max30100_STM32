/*
 * usb_task.c
 *
 *  Created on: May 14, 2026
 *      Author: serda
 */

#include "usb_task.h"
#include "usb_driver.h"
#include <cmsis_os.h>

void StartUsbTask(void const * argument)
{
	UsbMax30100Packet_t packet;
    UsbDriver_Init();

    osDelay(1000);

    for (;;)
    {
        if (UsbDriver_WaitAndCopyMax30100(&packet, 1000) == USB_DRV_OK)
        {
            UsbDriver_SendMax30100Packet(&packet);
        }

        osDelay(5);
    }
}
