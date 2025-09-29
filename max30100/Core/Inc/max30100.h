/*
 * max30100.h
 *
 *  Created on: Sep 18, 2025
 *      Author: serda
 */

#ifndef INC_MAX30100_H_
#define INC_MAX30100_H_

#include "main.h"
#include "stdbool.h"

#define MAX30100_I2C_DEV_ADDR  0xAE
#define MAX30100_INT_STATUS    0x00
#define MAX30100_INT_ENABLE    0x01
#define MAX30100_FIFO_WR_PTR   0x02
#define MAX30100_OVRFLOW_CNT   0x03
#define MAX30100_FIFO_RD_PTR   0x04
#define MAX30100_FIFO_DT_REG   0x05
#define MAX30100_MODE_CONFIG   0x06
#define MAX30100_SPO2_CONFIG   0x07
#define MAX30100_LED_CONFIG    0x09
#define MAX30100_TEMP_INT      0x16
#define MAX30100_TEMP_FRAC     0x17
#define MAX30100_REV_ID        0xFE
#define MAX30100_PART_ID       0xFF

#define MAX30100_TEMP_MODE     0x08
#define MAX30100_SPO2_HR_MODE  0x03
#define MAX30100_ONLY_HR_MODE  0x02

#define MEAN_FILTER_SIZE 15

#define PULSE_MAX_THRESHOLD  2000

#define PULSE_MIN_THRESHOLD             100
#define PULSE_MAX_THRESHOLD             2000
#define PULSE_GO_DOWN_THRESHOLD         1
#define PULSE_BPM_SAMPLE_SIZE           10

#define RESET_SPO2_EVERY_N_PULSES       100

#define RED_LED_CURRENT_ADJUSTMENT_MS   1000000
#define MAGIC_ACCEPTABLE_INTENSITY_DIFF 65000
#define RESET_SPO2_EVERY_N_PULSES       100

extern volatile uint32_t micsec;

typedef enum PulseStateMachine{
   PULSE_IDLE,
   PULSE_TRACE_UP,
   PULSE_TRACE_DOWN
}PulseStateMachine;

typedef struct {
	uint8_t REV_ID;
	uint8_t PART_ID;
} ID_MAX;

typedef struct {
	uint16_t raw_IR;
	uint16_t raw_RED;
} raw_data;

typedef struct {
	float md_IR;
	float md_RED;
} MD_data;

typedef struct {
	uint16_t lpbwf_IR;
	uint16_t lpbwf_RED;
} LPBWF_data;


typedef struct {
    float w;
    float result;
} dcFilter_t;

typedef struct {
  float values[MEAN_FILTER_SIZE];
  uint8_t index;
  float sum;
  uint8_t count;
} meanDiffFilter_t;

typedef struct {
	float v[2];
} butterworthFilter;

void MAX30100_WriteReg(uint8_t reg, uint8_t value);
uint8_t MAX30100_ReadReg(uint8_t reg);
ID_MAX Check_MAX(void);

void Init_MAX30100(void);
float read_tempMAX(void);
raw_data Read_MAX30100(void);
dcFilter_t dcRemoval(float x, float prev_w);
float MeanDiff(float M, meanDiffFilter_t* filterValues);
float LowPassButterWorthFilter(float x, butterworthFilter* filterResult);
void balanceIntesities(float redLedDC, float IRLedDC);
bool detectPulse(float sensor_value);

#endif /* INC_MAX30100_H_ */
