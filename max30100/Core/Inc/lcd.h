

#ifndef INC_LCD_H_
#define INC_LCD_H_

#include "main.h"

// RS determines What will LCD take, data or instruction. if RS = 0 its Instruction input, if RS = 1 its data input
#define RS_PIN GPIO_PIN_8
#define E_PIN  GPIO_PIN_9  // E_PIN determines its activity, if E = 0 it's non active, if E = 1 it's ready for working
#define D4_PIN GPIO_PIN_10
#define D5_PIN GPIO_PIN_11
#define D6_PIN GPIO_PIN_12
#define D7_PIN GPIO_PIN_13
#define LCD_PORT GPIOD

void LCD_EnablePulse(void);
void LCD_Send4Bit(uint8_t data);
void LCD_SendCmd(uint8_t cmd);
void LCD_SendData(uint8_t data);
void LCD_Init(void);
void LCD_Print(char *str);




#endif /* INC_LCD_H_ */
