/*
 * max_task.c
 *
 *  Created on: May 14, 2026
 *      Author: serda
 */
#include "max_task.h"
#include "max30100.h"
#include "usb_driver.h"
#include <cmsis_os.h>

void StartMaxTask(void const * argument)
{
  MAX30100_FilteredData max_data;
  UsbMax30100Packet_t usb_packet;

  while (Init_MAX30100( TEMP_DISABLE, MODE_SPO2_EN, SPO2_100, LED_PW_1600, RED_PA27_1, IR_PA27_1) != E_MAX_ERR_NONE)
	  osDelay(500);

  for(;;)
  {
      if (MAX30100_ReadFiltered(&max_data) == E_MAX_ERR_NONE)
      {
          usb_packet.bpm = max_data.bpm;
          usb_packet.spo2 = max_data.spo2;
          usb_packet.spo2_valid = max_data.spo2_valid ? 1 : 0;

          usb_packet.ppg_ir = max_data.ir_filtered;
          usb_packet.ppg_red = max_data.red_filtered;

          usb_packet.raw_ir = max_data.raw_ir;
          usb_packet.timestamp_ms = osKernelSysTick();

          UsbDriver_PublishMax30100(&usb_packet);
      }

      osDelay(10);
  }
}
