/*
 * max30100.c
 *
 *  Created on: Sep 18, 2025
 *      Author: serda
 */

#include "max30100.h"

extern HAL_StatusTypeDef cnt;
extern I2C_HandleTypeDef hi2c2;

void MAX30100_WriteReg(uint8_t reg, uint8_t value)
{
    cnt = HAL_I2C_Mem_Write(&hi2c2,
                      MAX30100_I2C_DEV_ADDR,
                      reg,
                      I2C_MEMADD_SIZE_8BIT,
                      &value,
                      1,
                      HAL_MAX_DELAY);
}

uint8_t MAX30100_ReadReg(uint8_t reg)
{
    uint8_t value;
    cnt = HAL_I2C_Mem_Read(&hi2c2,
                     MAX30100_I2C_DEV_ADDR,
                     reg,
                     I2C_MEMADD_SIZE_8BIT,
                     &value,
                     1,
                     HAL_MAX_DELAY);
    return value;
}

void Init_MAX30100(void)
{
    // 1. Reset sensör (MODE_CONFIG registerindeki reset biti ile)
    MAX30100_WriteReg(MAX30100_MODE_CONFIG, 0x40);
    HAL_Delay(500);

    MAX30100_WriteReg(MAX30100_MODE_CONFIG, MAX30100_SPO2_HR_MODE);
    HAL_Delay(500);

    // 3. SpO2 konfigürasyonu
    MAX30100_WriteReg(MAX30100_SPO2_CONFIG, 0x07);
    HAL_Delay(500);

    // 4. LED akımları ayarla (IR ve RED LED akımı)
    MAX30100_WriteReg(MAX30100_LED_CONFIG, 0x86);
    HAL_Delay(500);

    // 5. FIFO göstergelerini sıfırla
    MAX30100_WriteReg(MAX30100_FIFO_WR_PTR, 0x00);
    MAX30100_WriteReg(MAX30100_OVRFLOW_CNT, 0x00);
    MAX30100_WriteReg(MAX30100_FIFO_RD_PTR, 0x00);
    HAL_Delay(500);

    // 6. Interruptları aç (örn: yeni veri geldiğinde INT_ENABLE=0x10)
//    MAX30100_WriteReg(MAX30100_INT_ENABLE, 0x10);
}

ID_MAX Check_MAX()
{
	ID_MAX t;
	t.REV_ID = MAX30100_ReadReg(MAX30100_REV_ID);
	t.PART_ID = MAX30100_ReadReg(MAX30100_PART_ID);
	return t;
}

float read_tempMAX()
{
	uint8_t temp_integer = (int8_t)MAX30100_ReadReg(MAX30100_TEMP_INT);
	uint8_t temp_fraction = (int8_t)MAX30100_ReadReg(MAX30100_TEMP_FRAC);
	uint8_t frac_bits = temp_fraction & 0x0F  ;
	float temperature = temp_integer + (frac_bits * 0.0625f) ;
	return temperature ;
}

raw_data read_data;
uint8_t received_data[4];

raw_data Read_MAX30100()
{
	received_data[0] = MAX30100_ReadReg(MAX30100_FIFO_DT_REG);
	received_data[1] = MAX30100_ReadReg(MAX30100_FIFO_DT_REG);
	received_data[2] = MAX30100_ReadReg(MAX30100_FIFO_DT_REG);
	received_data[3] = MAX30100_ReadReg(MAX30100_FIFO_DT_REG);

	read_data.raw_IR = (received_data[0]<<8) | received_data[1];
	read_data.raw_RED = (received_data[2]<<8) | received_data[3];

	return read_data;
}

dcFilter_t dcRemoval(float x, float prev_w)
{
    dcFilter_t filtered;
    filtered.w = x + 0.95 * prev_w;
    filtered.result = filtered.w - prev_w;

    return filtered;
}

float MeanDiff(float M, meanDiffFilter_t* filterValues)
{
	float avg = 0;
	// 1) Pencereden çıkacak EN eski örneği toplamdan düş
	filterValues->sum -= filterValues->values[filterValues->index];

	// 2) Yeni örneği mevcut slot'a yaz
	filterValues->values[filterValues->index] = M;

	// 3) Toplama yeni örneği ekle
	filterValues->sum += filterValues->values[filterValues->index];

	// 4) Yazma işaretçisini bir ileri al
	filterValues->index++;
	filterValues->index = filterValues->index % MEAN_FILTER_SIZE;

	// 5) Isınma: pencere dolana kadar bölen 'count'u artır
	if(filterValues->count < MEAN_FILTER_SIZE)
		filterValues->count++;

	// 6) Anlık ortalamayı hesapla
	avg = filterValues->sum / filterValues->count;

	// 7) Çıkış: ORTALAMA - MEVCUT
	return avg - M;
}

float LowPassButterWorthFilter(float x, butterworthFilter* filterResult)
{
	filterResult->v[0] = filterResult->v[1];

	//Fs = 100Hz and Fc= 10Hz
	filterResult->v[1] = (2.452372752527856026e-1 * x) + (0.50952544949442879485 * filterResult->v[0]);

//	Fs = 100Hz and Fc = 4Hz
//	filterResult->v[1] = (1.367287359973195227e-1 * x) + (0.72654252800536101020 * filterResult->v[0]);

	return filterResult->v[0] + filterResult->v[1];
}

uint8_t currentPulseDetectorState = PULSE_IDLE;
uint32_t lastBeatThreshold = 0;
uint32_t currentBPM;
uint32_t valuesBPM[PULSE_BPM_SAMPLE_SIZE] = {0};
uint32_t valuesBPMSum = 0;
uint8_t valuesBPMCount = 0;
uint8_t bpmIndex = 0;

bool detectPulse(float sensor_value)
{
  static float prev_sensor_value = 0;
  static uint8_t values_went_down = 0;
  static uint32_t currentBeat = 0;
  static uint32_t lastBeat = 0;

  if(sensor_value > PULSE_MAX_THRESHOLD)
  {
	currentPulseDetectorState = PULSE_IDLE;
	prev_sensor_value = 0;
	lastBeat = 0;
	currentBeat = 0;
	values_went_down = 0;
	lastBeatThreshold = 0;
	return false;
  }

  switch(currentPulseDetectorState)
  {
	case PULSE_IDLE:
	  if(sensor_value >= PULSE_MIN_THRESHOLD) {
		currentPulseDetectorState = PULSE_TRACE_UP;
		values_went_down = 0;
	  }
	  break;

	case PULSE_TRACE_UP:
	  if(sensor_value > prev_sensor_value)
	  {
		currentBeat = micsec;
		lastBeatThreshold = sensor_value;
	  }
	  else
	  {

		uint32_t beatDuration = currentBeat - lastBeat;
		lastBeat = currentBeat;

		float rawBPM = 0;
		if(beatDuration > 0)
		  rawBPM = 60000.0f / (float)beatDuration;  //0000 de olabilir

		valuesBPM[bpmIndex] = rawBPM;
		valuesBPMSum = 0;
		for(int i=0; i<PULSE_BPM_SAMPLE_SIZE; i++)
		{
		  valuesBPMSum += valuesBPM[i];
		}

		bpmIndex++;
		bpmIndex = bpmIndex % PULSE_BPM_SAMPLE_SIZE;

		if(valuesBPMCount < PULSE_BPM_SAMPLE_SIZE)
		  valuesBPMCount++;

		currentBPM = valuesBPMSum / valuesBPMCount;

		currentPulseDetectorState = PULSE_TRACE_DOWN;

		return true;
	  }
	  break;

	case PULSE_TRACE_DOWN:
	  if(sensor_value < prev_sensor_value)
	  {
		values_went_down++;
	  }


	  if(sensor_value < PULSE_MIN_THRESHOLD)
	  {
		currentPulseDetectorState = PULSE_IDLE;
	  }
	  break;
  }

  prev_sensor_value = sensor_value;
  return false;
}

volatile uint32_t lastREDLedCurrentCheck = 0;
static const uint8_t MAX30100_LED_CURRENT_50MA = 15;
static const uint8_t FIX_IR_CURRENT = 6;
uint8_t redLEDCurrent = 5;

void balanceIntesities(float redLedDC, float IRLedDC)
{
	if( micsec - lastREDLedCurrentCheck >= RED_LED_CURRENT_ADJUSTMENT_MS)
	  {
	    if( IRLedDC - redLedDC > MAGIC_ACCEPTABLE_INTENSITY_DIFF && redLEDCurrent < MAX30100_LED_CURRENT_50MA)
	    {
	      redLEDCurrent++;
	      MAX30100_WriteReg(MAX30100_LED_CONFIG, (redLEDCurrent << 4) | FIX_IR_CURRENT);
	    }
	    else if(redLedDC - IRLedDC > MAGIC_ACCEPTABLE_INTENSITY_DIFF && redLEDCurrent > 0)
	    {
	      redLEDCurrent--;
	      MAX30100_WriteReg(MAX30100_LED_CONFIG, (redLEDCurrent << 4) | FIX_IR_CURRENT);
	    }
	    lastREDLedCurrentCheck = micsec;
	  }
}









