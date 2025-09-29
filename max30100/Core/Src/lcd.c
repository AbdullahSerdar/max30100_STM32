/*
 * lcd.c
 *
 *  Created on: Sep 18, 2025
 *      Author: serda
 */

#include "lcd.h"

// All the time whenever we send data or instruction, we need to turn on and off E pin for activating sending process
void LCD_EnablePulse(void) {
    HAL_GPIO_WritePin(LCD_PORT, E_PIN, GPIO_PIN_SET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(LCD_PORT, E_PIN, GPIO_PIN_RESET);
    HAL_Delay(1);
}

// struct for sending data in two piece, each have 4 bits
void LCD_Send4Bit(uint8_t data) {
    HAL_GPIO_WritePin(LCD_PORT, D4_PIN, (data >> 0) & 0x01);
    HAL_GPIO_WritePin(LCD_PORT, D5_PIN, (data >> 1) & 0x01);
    HAL_GPIO_WritePin(LCD_PORT, D6_PIN, (data >> 2) & 0x01);
    HAL_GPIO_WritePin(LCD_PORT, D7_PIN, (data >> 3) & 0x01);
}

void LCD_SendCmd(uint8_t cmd) {
    HAL_GPIO_WritePin(LCD_PORT, RS_PIN, GPIO_PIN_RESET); // RS = 0 → Instruction
    LCD_Send4Bit(cmd >> 4);  // shift 4 and send up bits (0x28 = 0010 1000) -> 0000 0010
    LCD_EnablePulse();
    LCD_Send4Bit(cmd & 0x0F); // send lower bits (0010 1000 & 0000 1111) -> 1000
    LCD_EnablePulse();
    HAL_Delay(2);
}

void LCD_SendData(uint8_t data) {
    HAL_GPIO_WritePin(LCD_PORT, RS_PIN, GPIO_PIN_SET); // RS = 1 → Data
    LCD_Send4Bit(data >> 4);
    LCD_EnablePulse();
    LCD_Send4Bit(data & 0x0F);
    LCD_EnablePulse();
    HAL_Delay(2);
}

void LCD_Init(void) {
    HAL_Delay(50);        // LCD ilk açılış gecikmesi
    LCD_SendCmd(0x33);    // Başlat
    LCD_SendCmd(0x32);    // 4-bit moda geç
    LCD_SendCmd(0x28);    // 4-bit, 2 satır, 5x8 font
    LCD_SendCmd(0x0C);    // Display ON, Cursor OFF
    LCD_SendCmd(0x06);    // Auto-increment cursor
    LCD_SendCmd(0x01);    // Clear display
    HAL_Delay(2);
}

void LCD_Print(char *str) {
    while(*str) {
        LCD_SendData(*str++);
    }
}
