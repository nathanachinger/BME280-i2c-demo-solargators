/*
 * BME280 Humidity, Pressure, Temp Sensor I2C Driver
 *
 * Author: Nate Achinger
 * Created: 3/30/2026
 */

#pragma once

#include "stm32l4xx_hal.h"

#define BME280_CHIP_ID 0x60

// REGISTER MAP
#define BME280_CHIP_ID_REG 0xD0
#define BME280_RESET_REG 0xE0
#define BME280_CONFIG_REG 0xF5

#define BME280_CTRL_MEAS_REG    0xF4
#define BME280_CTRL_HUM_REG     0xF2

#define BME280_DATA_REGS_START 0xF7

#define BME280_CALIB_TP_BASE 0x88    // T1-T3 & P1-P9, 24 bytes
#define BME280_CALIB_H1_BASE 0xA1    // H1, 1 byte
#define BME280_CALIB_H2_BASE 0xE1    // H2-H6, 7 bytes

typedef struct {
    I2C_HandleTypeDef* hi2c;
    uint8_t i2c_addr;

    // internal calculation value
    // shared across all three compensation functions
    int32_t t_fine;

    // COMPENSATION VALUES
    // Temperature
    uint16_t dig_T1;
    int16_t  dig_T2;
    int16_t  dig_T3;

    // Pressure
    uint16_t dig_P1;
    int16_t  dig_P2;
    int16_t  dig_P3;
    int16_t  dig_P4;
    int16_t  dig_P5;
    int16_t  dig_P6;
    int16_t  dig_P7;
    int16_t  dig_P8;
    int16_t  dig_P9;

    // Humidity
    uint8_t  dig_H1;
    int16_t  dig_H2;
    uint8_t  dig_H3;
    int16_t  dig_H4;
    int16_t  dig_H5;
    int8_t   dig_H6;

} BME280_Handle;

uint8_t BME280_Init(BME280_Handle *dev, I2C_HandleTypeDef *hi2c, uint8_t addr);

// Temp in degrees C, pressure in Pa, Humidity in %RH
void BME280_ReadAll(BME280_Handle *dev, float *temp, float *pressure, float *humidity);

