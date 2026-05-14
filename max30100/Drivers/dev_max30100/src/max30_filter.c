/*
 * max30_filter.c
 *
 *  Created on: May 14, 2026
 *      Author: serda
 */

#include "max30_filter.h"
#include "max30100.h"
#include "max30100_hal.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MAX30_DC_CUTOFF_HZ          0.50f
#define MAX30_LP_CUTOFF_HZ          5.00f
#define MAX30_PEAK_ENV_ALPHA        0.01f
#define MAX30_MIN_PULSE_AMPLITUDE   20.0f
#define MAX30_THRESHOLD_RATIO       0.60f
#define MAX30_SPO2_WINDOW_SEC       1.00f
#define MAX30_LED_CHECK_PERIOD_MS   1000U
#define MAX30_LED_DIFF_LIMIT        65000.0f
#define MAX30_RED_LED_MAX_CURRENT   15U
#define MAX30_FIXED_IR_CURRENT      6U

typedef struct {
    float y;
    float alpha;
} OnePoleLPF;

typedef struct {
    float sample_rate_hz;
    uint32_t sample_index;

    OnePoleLPF ir_dc;
    OnePoleLPF red_dc;
    OnePoleLPF ir_lpf;
    OnePoleLPF red_lpf;

    float env_min;
    float env_max;
    bool was_above_threshold;
    uint32_t last_beat_sample;

    float bpm_values[MAX30_BPM_AVG_SIZE];
    uint8_t bpm_index;
    uint8_t bpm_count;
    float bpm_sum;

    float ir_ac_sq_sum;
    float red_ac_sq_sum;
    uint16_t spo2_count;
    uint16_t spo2_window_samples;

    MAX30100_FilteredData last;
} Max30FilterState;

static Max30FilterState g_filter;
static uint8_t g_red_led_current = 5U;

static float clampf_local(float x, float min_v, float max_v)
{
    if (x < min_v) return min_v;
    if (x > max_v) return max_v;
    return x;
}

static float one_pole_alpha(float cutoff_hz, float sample_rate_hz)
{
    if (cutoff_hz <= 0.0f || sample_rate_hz <= 0.0f) {
        return 1.0f;
    }

    const float dt = 1.0f / sample_rate_hz;
    const float rc = 1.0f / (2.0f * (float)M_PI * cutoff_hz);
    return dt / (rc + dt);
}

static float lpf_apply(OnePoleLPF *f, float x)
{
    f->y += f->alpha * (x - f->y);
    return f->y;
}

static void bpm_push(float bpm)
{
    if (g_filter.bpm_count < MAX30_BPM_AVG_SIZE) {
        g_filter.bpm_values[g_filter.bpm_index] = bpm;
        g_filter.bpm_sum += bpm;
        g_filter.bpm_count++;
    } else {
        g_filter.bpm_sum -= g_filter.bpm_values[g_filter.bpm_index];
        g_filter.bpm_values[g_filter.bpm_index] = bpm;
        g_filter.bpm_sum += bpm;
    }

    g_filter.bpm_index++;
    if (g_filter.bpm_index >= MAX30_BPM_AVG_SIZE) {
        g_filter.bpm_index = 0U;
    }

    g_filter.last.bpm = g_filter.bpm_sum / (float)g_filter.bpm_count;
}

void Max30Filter_Init(float sample_rate_hz)
{
    if (sample_rate_hz <= 0.0f) {
        sample_rate_hz = MAX30_DEFAULT_SAMPLE_RATE_HZ;
    }

    memset(&g_filter, 0, sizeof(g_filter));
    g_filter.sample_rate_hz = sample_rate_hz;

    g_filter.ir_dc.alpha  = one_pole_alpha(MAX30_DC_CUTOFF_HZ, sample_rate_hz);
    g_filter.red_dc.alpha = one_pole_alpha(MAX30_DC_CUTOFF_HZ, sample_rate_hz);
    g_filter.ir_lpf.alpha = one_pole_alpha(MAX30_LP_CUTOFF_HZ, sample_rate_hz);
    g_filter.red_lpf.alpha = one_pole_alpha(MAX30_LP_CUTOFF_HZ, sample_rate_hz);

    g_filter.env_min = 0.0f;
    g_filter.env_max = 0.0f;

    g_filter.spo2_window_samples = (uint16_t)(sample_rate_hz * MAX30_SPO2_WINDOW_SEC);
    if (g_filter.spo2_window_samples < 25U) {
        g_filter.spo2_window_samples = 25U;
    }
}

void Max30Filter_Reset(void)
{
    const float sr = (g_filter.sample_rate_hz > 0.0f) ? g_filter.sample_rate_hz : MAX30_DEFAULT_SAMPLE_RATE_HZ;
    Max30Filter_Init(sr);
}

MAX30100_FilteredData Max30Filter_GetLast(void)
{
    return g_filter.last;
}

MAX30100_FilteredData Max30Filter_Process(uint16_t raw_ir, uint16_t raw_red)
{
    MAX30100_FilteredData *out = &g_filter.last;
    out->raw_ir = raw_ir;
    out->raw_red = raw_red;
    out->beat_detected = false;

    if (g_filter.sample_rate_hz <= 0.0f) {
        Max30Filter_Init(MAX30_DEFAULT_SAMPLE_RATE_HZ);
    }

    const float ir = (float)raw_ir;
    const float red = (float)raw_red;

    out->ir_dc = lpf_apply(&g_filter.ir_dc, ir);
    out->red_dc = lpf_apply(&g_filter.red_dc, red);

    out->ir_ac = ir - out->ir_dc;
    out->red_ac = red - out->red_dc;

    out->ir_filtered = lpf_apply(&g_filter.ir_lpf, out->ir_ac);
    out->red_filtered = lpf_apply(&g_filter.red_lpf, out->red_ac);

    /* HR is more stable from IR channel. Use an adaptive threshold. */
    const float sig = out->ir_filtered;
    if (g_filter.sample_index < 5U) {
        g_filter.env_min = sig;
        g_filter.env_max = sig;
    } else {
        g_filter.env_min += MAX30_PEAK_ENV_ALPHA * (sig - g_filter.env_min);
        g_filter.env_max += MAX30_PEAK_ENV_ALPHA * (sig - g_filter.env_max);
        if (sig < g_filter.env_min) g_filter.env_min = sig;
        if (sig > g_filter.env_max) g_filter.env_max = sig;
    }

    const float amplitude = g_filter.env_max - g_filter.env_min;
    const float threshold = g_filter.env_min + (MAX30_THRESHOLD_RATIO * amplitude);
    const bool above = (sig > threshold) && (amplitude > MAX30_MIN_PULSE_AMPLITUDE);

    const uint32_t min_samples_between_beats = (uint32_t)((60.0f / MAX30_HR_MAX_BPM) * g_filter.sample_rate_hz);
    const uint32_t max_samples_between_beats = (uint32_t)((60.0f / MAX30_HR_MIN_BPM) * g_filter.sample_rate_hz);

    if (above && !g_filter.was_above_threshold) {
        const uint32_t diff = g_filter.sample_index - g_filter.last_beat_sample;

        if (g_filter.last_beat_sample != 0U && diff >= min_samples_between_beats && diff <= max_samples_between_beats) {
            const float bpm = 60.0f * g_filter.sample_rate_hz / (float)diff;
            bpm_push(bpm);
            out->beat_detected = true;
        }

        g_filter.last_beat_sample = g_filter.sample_index;
    }
    g_filter.was_above_threshold = above;

    /* SpO2 ratio-of-ratios over a short RMS window. */
    g_filter.ir_ac_sq_sum += out->ir_filtered * out->ir_filtered;
    g_filter.red_ac_sq_sum += out->red_filtered * out->red_filtered;
    g_filter.spo2_count++;

    if (g_filter.spo2_count >= g_filter.spo2_window_samples) {
        const float ir_rms = sqrtf(g_filter.ir_ac_sq_sum / (float)g_filter.spo2_count);
        const float red_rms = sqrtf(g_filter.red_ac_sq_sum / (float)g_filter.spo2_count);

        if (out->ir_dc > 1.0f && out->red_dc > 1.0f && ir_rms > 0.001f) {
            out->ratio_r = (red_rms / out->red_dc) / (ir_rms / out->ir_dc);
            out->spo2 = clampf_local(110.0f - (25.0f * out->ratio_r), 0.0f, 100.0f);
            out->spo2_valid = true;
        } else {
            out->spo2_valid = false;
        }

        g_filter.ir_ac_sq_sum = 0.0f;
        g_filter.red_ac_sq_sum = 0.0f;
        g_filter.spo2_count = 0U;
    }

    g_filter.sample_index++;
    return *out;
}

bool detectPulse(float sensor_value)
{
    /* Kept only for old code compatibility. New code should use Max30Filter_Process(). */
    static bool was_above = false;
    static float env_min = 0.0f;
    static float env_max = 0.0f;
    static bool initialized = false;

    if (!initialized) {
        env_min = sensor_value;
        env_max = sensor_value;
        initialized = true;
    }

    env_min += MAX30_PEAK_ENV_ALPHA * (sensor_value - env_min);
    env_max += MAX30_PEAK_ENV_ALPHA * (sensor_value - env_max);
    if (sensor_value < env_min) env_min = sensor_value;
    if (sensor_value > env_max) env_max = sensor_value;

    const float amplitude = env_max - env_min;
    const float threshold = env_min + (MAX30_THRESHOLD_RATIO * amplitude);
    const bool above = (sensor_value > threshold) && (amplitude > MAX30_MIN_PULSE_AMPLITUDE);
    const bool pulse = above && !was_above;
    was_above = above;
    return pulse;
}

void balanceIntesities(float redLedDC, float IRLedDC)
{
    static uint32_t last_check_ms = 0U;
    const uint32_t now_ms = HAL_GetTick();

    if ((now_ms - last_check_ms) < MAX30_LED_CHECK_PERIOD_MS) {
        return;
    }

    if ((IRLedDC - redLedDC) > MAX30_LED_DIFF_LIMIT && g_red_led_current < MAX30_RED_LED_MAX_CURRENT) {
        g_red_led_current++;
    } else if ((redLedDC - IRLedDC) > MAX30_LED_DIFF_LIMIT && g_red_led_current > 0U) {
        g_red_led_current--;
    } else {
        last_check_ms = now_ms;
        return;
    }

    const uint8_t data = (uint8_t)((g_red_led_current << 4) | MAX30_FIXED_IR_CURRENT);
    (void)HAL_I2C_Mem_Write(MAX_I2C_PORT, MAX30100_I2C_DEV_ADDR, MAX30100_LED_CONFIG,
                            I2C_MEMADD_SIZE_8BIT, (uint8_t *)&data, 1, 100);

    last_check_ms = now_ms;
}
