/*
 * max30_filter.h
 *
 *  Created on: May 14, 2026
 *      Author: serda
 */

#ifndef DEV_MAX30100_INC_MAX30_FILTER_H_
#define DEV_MAX30100_INC_MAX30_FILTER_H_

#include <stdbool.h>
#include <stdint.h>

#define MAX30_BPM_AVG_SIZE           8U
#define MAX30_DEFAULT_SAMPLE_RATE_HZ 100.0f
#define MAX30_HR_MIN_BPM             40.0f
#define MAX30_HR_MAX_BPM             220.0f

typedef struct {
    uint16_t raw_ir;
    uint16_t raw_red;

    float ir_dc;
    float red_dc;

    float ir_ac;
    float red_ac;

    float ir_filtered;
    float red_filtered;

    bool beat_detected;
    float bpm;

    float ratio_r;
    float spo2;
    bool spo2_valid;
} MAX30100_FilteredData;

void Max30Filter_Init(float sample_rate_hz);
void Max30Filter_Reset(void);
MAX30100_FilteredData Max30Filter_Process(uint16_t raw_ir, uint16_t raw_red);
MAX30100_FilteredData Max30Filter_GetLast(void);

/* Backward-compatible helpers. Prefer Max30Filter_Process() in new code. */
bool detectPulse(float sensor_value);
void balanceIntesities(float redLedDC, float IRLedDC);

#endif /* DEV_MAX30100_INC_MAX30_FILTER_H_ */
