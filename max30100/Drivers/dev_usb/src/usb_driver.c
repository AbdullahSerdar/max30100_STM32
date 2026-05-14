#include "usb_driver.h"

#include "cmsis_os.h"
#include "usbd_cdc_if.h"

#include <stdio.h>
#include <string.h>

extern osMutexId usbMutexHandle;
extern osSemaphoreId usbSemaphoreHandle;

static UsbMax30100Packet_t latestMaxPacket;
static uint8_t hasPacket = 0;

void UsbDriver_Init(void)
{
    hasPacket = 0;

    if (usbSemaphoreHandle != NULL)
    {
        osSemaphoreWait(usbSemaphoreHandle, 0);
    }
}

UsbDriver_Status_t UsbDriver_PublishMax30100(const UsbMax30100Packet_t *packet)
{
    if (packet == NULL)
    {
        return USB_DRV_ERROR;
    }

    if (usbMutexHandle == NULL || usbSemaphoreHandle == NULL)
    {
        return USB_DRV_NOT_READY;
    }

    if (osMutexWait(usbMutexHandle, 20) != osOK)
    {
        return USB_DRV_BUSY;
    }

    latestMaxPacket = *packet;
    hasPacket = 1;

    osMutexRelease(usbMutexHandle);

    osSemaphoreRelease(usbSemaphoreHandle);

    return USB_DRV_OK;
}

UsbDriver_Status_t UsbDriver_WaitAndCopyMax30100(UsbMax30100Packet_t *packet,
                                                 uint32_t timeout_ms)
{
    if (packet == NULL)
    {
        return USB_DRV_ERROR;
    }

    if (usbMutexHandle == NULL || usbSemaphoreHandle == NULL)
    {
        return USB_DRV_NOT_READY;
    }

    if (osSemaphoreWait(usbSemaphoreHandle, timeout_ms) != osOK)
    {
        return USB_DRV_BUSY;
    }

    if (osMutexWait(usbMutexHandle, 20) != osOK)
    {
        return USB_DRV_BUSY;
    }

    if (!hasPacket)
    {
        osMutexRelease(usbMutexHandle);
        return USB_DRV_ERROR;
    }

    *packet = latestMaxPacket;

    osMutexRelease(usbMutexHandle);

    return USB_DRV_OK;
}

UsbDriver_Status_t UsbDriver_SendMax30100Packet(const UsbMax30100Packet_t *packet)
{
    if (packet == NULL)
    {
        return USB_DRV_ERROR;
    }

    char txBuf[160];

    int len = snprintf(txBuf,
                       sizeof(txBuf),
                       "MAX30100,%.2f,%.2f,%u,%.2f,%.2f,%u,%lu\r\n",
                       packet->bpm,
                       packet->spo2,
                       packet->spo2_valid,
                       packet->ppg_ir,
                       packet->ppg_red,
					   packet->raw_ir,
                       packet->timestamp_ms);

    if (len <= 0 || len >= (int)sizeof(txBuf))
    {
        return USB_DRV_ERROR;
    }

    /*
     * USB CDC bazen meşgul olabilir.
     * Bu yüzden küçük retry ekliyoruz.
     */
    for (uint8_t retry = 0; retry < 5; retry++)
    {
        uint8_t result = CDC_Transmit_FS((uint8_t *)txBuf, (uint16_t)len);

        if (result == USBD_OK)
        {
            return USB_DRV_OK;
        }

        osDelay(5);
    }

    return USB_DRV_BUSY;
}
