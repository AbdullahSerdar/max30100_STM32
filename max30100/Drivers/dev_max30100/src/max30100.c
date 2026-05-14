#include "max30100.h"
#include "max30100_hal.h"
#include <cmsis_os.h>

static ID_MAX max30_id;
static TEMP_VALUES temp;
static double temperature;
static raw_data raw_ir_red;

#define CAL_TEMP(temp_int, temp_frac)  ((double)(temp_int) + ((double)(temp_frac) * 0.0625))

static err_MaxErrors hal_to_max_error(HAL_StatusTypeDef hal)
{
    return (hal == HAL_OK) ? E_MAX_ERR_NONE : E_MAX_ERR_HAL;
}

static float sample_rate_to_hz(uint8_t spo2_conf)
{
    switch (spo2_conf & 0x07U) {
        case SPO2_50:   return 50.0f;
        case SPO2_100:  return 100.0f;
        case SPO2_167:  return 167.0f;
        case SPO2_200:  return 200.0f;
        case SPO2_400:  return 400.0f;
        case SPO2_600:  return 600.0f;
        case SPO2_800:  return 800.0f;
        case SPO2_1000: return 1000.0f;
        default:        return 100.0f;
    }
}

static err_MaxErrors write_reg(uint8_t reg, uint8_t value)
{
    return hal_to_max_error(HAL_I2C_Mem_Write(MAX_I2C_PORT, MAX30100_I2C_DEV_ADDR,
                                              reg, I2C_MEMADD_SIZE_8BIT,
                                              &value, 1, 100));
}

err_MaxErrors Init_MAX30100(uint8_t temp_en, uint8_t mode_sel, uint8_t spo2_conf,
                            uint8_t led_pw, uint8_t red_pa, uint8_t ir_pa)
{
    err_MaxErrors status;

    if (mode_sel != MODE_HR_ONLY && mode_sel != MODE_SPO2_EN) {
        return E_MAX_ERR_PARAM;
    }

    /* Reset device. */
    status = write_reg(MAX30100_MODE_CONFIG, 0x40U);
    if (status != E_MAX_ERR_NONE) { return status; }
    osDelay(100);

    status = Check_MAX30100(&max30_id);
    if (status != E_MAX_ERR_NONE) { return status; }
    if (max30_id.PART_ID != MAX30100_EXPECTED_PART_ID) { return E_MAX_ERR_WRONG_ID; }

    status = write_reg(MAX30100_FIFO_WR_PTR, 0x00U);
    if (status != E_MAX_ERR_NONE) { return status; }
    status = write_reg(MAX30100_OVRFLOW_CNT, 0x00U);
    if (status != E_MAX_ERR_NONE) { return status; }
    status = write_reg(MAX30100_FIFO_RD_PTR, 0x00U);
    if (status != E_MAX_ERR_NONE) { return status; }

    status = write_reg(MAX30100_MODE_CONFIG, (uint8_t)(((temp_en & 0x01U) << 3) | (mode_sel & 0x07U)));
    if (status != E_MAX_ERR_NONE) { return status; }

    status = write_reg(MAX30100_SPO2_CONFIG, (uint8_t)(0x40U | ((spo2_conf & 0x07U) << 2) | (led_pw & 0x03U)));
    if (status != E_MAX_ERR_NONE) { return status; }

    status = write_reg(MAX30100_LED_CONFIG, (uint8_t)(((red_pa & 0x0FU) << 4) | (ir_pa & 0x0FU)));
    if (status != E_MAX_ERR_NONE) { return status; }

    Max30Filter_Init(sample_rate_to_hz(spo2_conf));

    return E_MAX_ERR_NONE;
}

err_MaxErrors Check_MAX30100(ID_MAX *id)
{
    if (id == 0) {
        return E_MAX_ERR_PARAM;
    }

    uint8_t value[2] = {0};
    HAL_StatusTypeDef hal = HAL_I2C_Mem_Read(MAX_I2C_PORT, MAX30100_I2C_DEV_ADDR,
                                             MAX30100_REV_ID, I2C_MEMADD_SIZE_8BIT,
                                             value, 2, 100);
    if (hal != HAL_OK) { return E_MAX_ERR_HAL; }

    id->REV_ID  = value[0];
    id->PART_ID = value[1];
    max30_id = *id;

    return E_MAX_ERR_NONE;
}

err_MaxErrors Read_MAX_Temp(void)
{
    uint8_t temp_val[2] = {0};
    HAL_StatusTypeDef hal = HAL_I2C_Mem_Read(MAX_I2C_PORT, MAX30100_I2C_DEV_ADDR,
                                             MAX30100_TEMP_INT, I2C_MEMADD_SIZE_8BIT,
                                             temp_val, 2, 100);
    if (hal != HAL_OK) { return E_MAX_ERR_HAL; }

    temp.temp_int  = (int8_t)temp_val[0];
    temp.temp_frac = temp_val[1] & 0x0FU;
    temperature = CAL_TEMP(temp.temp_int, temp.temp_frac);

    return E_MAX_ERR_NONE;
}

err_MaxErrors Read_MAX_Ir_Red(void)
{
    uint8_t ir_red_datas[4] = {0};

    HAL_StatusTypeDef hal = HAL_I2C_Mem_Read(MAX_I2C_PORT, MAX30100_I2C_DEV_ADDR,
                                             MAX30100_FIFO_DT_REG, I2C_MEMADD_SIZE_8BIT,
                                             ir_red_datas, 4, 100);
    if (hal != HAL_OK) { return E_MAX_ERR_HAL; }

    raw_ir_red.raw_IR  = (uint16_t)(((uint16_t)ir_red_datas[0] << 8) | ir_red_datas[1]);
    raw_ir_red.raw_RED = (uint16_t)(((uint16_t)ir_red_datas[2] << 8) | ir_red_datas[3]);

    return E_MAX_ERR_NONE;
}

err_MaxErrors MAX30100_ReadFiltered(MAX30100_FilteredData *out)
{
    if (out == 0) {
        return E_MAX_ERR_PARAM;
    }

    err_MaxErrors status = Read_MAX_Ir_Red();
    if (status != E_MAX_ERR_NONE) {
        return status;
    }

    *out = Max30Filter_Process(raw_ir_red.raw_IR, raw_ir_red.raw_RED);
    balanceIntesities(out->red_dc, out->ir_dc);

    return E_MAX_ERR_NONE;
}

raw_data MAX30100_GetRaw(void)
{
    return raw_ir_red;
}

double MAX30100_GetTemperature(void)
{
    return temperature;
}

ID_MAX MAX30100_GetId(void)
{
    return max30_id;
}
