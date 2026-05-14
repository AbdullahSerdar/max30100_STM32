#ifndef INC_MAX30100_H_
#define INC_MAX30100_H_


#include "main.h"
#include "max30_filter.h"

#define MAX30100_I2C_DEV_ADDR  (0x57U << 1)

#define MAX30100_INT_STATUS    0x00U
#define MAX30100_INT_ENABLE    0x01U
#define MAX30100_FIFO_WR_PTR   0x02U
#define MAX30100_OVRFLOW_CNT   0x03U
#define MAX30100_FIFO_RD_PTR   0x04U
#define MAX30100_FIFO_DT_REG   0x05U
#define MAX30100_MODE_CONFIG   0x06U
#define MAX30100_SPO2_CONFIG   0x07U
#define MAX30100_LED_CONFIG    0x09U
#define MAX30100_TEMP_INT      0x16U
#define MAX30100_TEMP_FRAC     0x17U
#define MAX30100_REV_ID        0xFEU
#define MAX30100_PART_ID       0xFFU

#define MAX30100_EXPECTED_PART_ID 0x11U

#define TEMP_ENABLE     1U
#define TEMP_DISABLE    0U

#define MODE_HR_ONLY    0x02U
#define MODE_SPO2_EN    0x03U

#define SPO2_50         0x00U
#define SPO2_100        0x01U
#define SPO2_167        0x02U
#define SPO2_200        0x03U
#define SPO2_400        0x04U
#define SPO2_600        0x05U
#define SPO2_800        0x06U
#define SPO2_1000       0x07U

#define LED_PW_200      0x00U
#define LED_PW_400      0x01U
#define LED_PW_800      0x02U
#define LED_PW_1600     0x03U

#define RED_PA0_0       0x00U
#define RED_PA4_4       0x01U
#define RED_PA7_6       0x02U
#define RED_PA11_0      0x03U
#define RED_PA14_2      0x04U
#define RED_PA17_4      0x05U
#define RED_PA20_8      0x06U
#define RED_PA24_0      0x07U
#define RED_PA27_1      0x08U
#define RED_PA30_6      0x09U
#define RED_PA33_8      0x0AU
#define RED_PA37_0      0x0BU
#define RED_PA40_2      0x0CU
#define RED_PA43_6      0x0DU
#define RED_PA46_8      0x0EU
#define RED_PA50_0      0x0FU

#define IR_PA0_0        0x00U
#define IR_PA4_4        0x01U
#define IR_PA7_6        0x02U
#define IR_PA11_0       0x03U
#define IR_PA14_2       0x04U
#define IR_PA17_4       0x05U
#define IR_PA20_8       0x06U
#define IR_PA24_0       0x07U
#define IR_PA27_1       0x08U
#define IR_PA30_6       0x09U
#define IR_PA33_8       0x0AU
#define IR_PA37_0       0x0BU
#define IR_PA40_2       0x0CU
#define IR_PA43_6       0x0DU
#define IR_PA46_8       0x0EU
#define IR_PA50_0       0x0FU

typedef struct {
    uint8_t REV_ID;
    uint8_t PART_ID;
} ID_MAX;

typedef struct {
    int8_t temp_int;
    uint8_t temp_frac;
} TEMP_VALUES;

typedef struct {
    uint16_t raw_IR;
    uint16_t raw_RED;
} raw_data;

typedef enum
{
    E_MAX_ERR_NONE = 0,
    E_MAX_ERR_WRONG_ID,
    E_MAX_ERR_HAL,
    E_MAX_ERR_PARAM,
    E_MAX_ERR_UNKNOWN
} err_MaxErrors;

err_MaxErrors Init_MAX30100(uint8_t temp_en, uint8_t mode_sel, uint8_t spo2_conf,
                            uint8_t led_pw, uint8_t red_pa, uint8_t ir_pa);
err_MaxErrors Check_MAX30100(ID_MAX *max_id);
err_MaxErrors Read_MAX_Temp(void);
err_MaxErrors Read_MAX_Ir_Red(void);
err_MaxErrors MAX30100_ReadFiltered(MAX30100_FilteredData *out);

raw_data MAX30100_GetRaw(void);
double MAX30100_GetTemperature(void);
ID_MAX MAX30100_GetId(void);


#endif /* INC_MAX30100_H_ */
