#include "BME280.h"

uint8_t BME280_Init(BME280_Handle *dev, I2C_HandleTypeDef *hi2c, uint8_t addr)
{
    dev->hi2c = hi2c;
    dev->i2c_addr = (addr << 1);

    uint8_t read_chip_id;
    BME280_ReadRegister(dev, BME280_CHIP_ID_REG, &read_chip_id);

    // BME280 not found
    if (read_chip_id != BME280_CHIP_ID)
    {
        return 0;
    }

    BME280_ReadCalibrationData(dev);

    uint8_t data;

    // Set humidity oversampling x1
    data = 0x01;
    BME280_WriteRegister(dev, BME280_CTRL_HUM_REG, &data);

    // Set temp and pressure oversampling x1, normal mode
    data = (0x01 << 5) |  // osrs_t = 001 (x1)
           (0x01 << 2) |  // osrs_p = 001 (x1)
            0x03;         // mode   = 11  (normal)
    BME280_WriteRegister(dev, BME280_CTRL_MEAS_REG, &data);
}

void BME280_ReadAll(BME280_Handle *dev, float *temp, float *pressure, float *humidity)
{
    int32_t adc_T, adc_P, adc_H;
    BME280_ReadRawData(dev, &adc_T, &adc_P, &adc_H);

    // Temperature must be first to populate t_fine
    *temp     = BME280_CompensateTemperature(dev, adc_T) / 100.0f;
    *pressure = BME280_CompensatePressure(dev, adc_P)    / 100.0f;  // Pa → hPa
    *humidity = BME280_CompensateHumidity(dev, adc_H)    / 1024.0f;
}

void BME280_ReadRawData(BME280_Handle *dev, int32_t *adc_T, int32_t *adc_P, int32_t *adc_H)
{
    uint8_t buf[8];
    BME280_ReadRegisters(dev, BME280_DATA_REGS_START, buf, 8);

    // Pressure: 0xF7[7:0] + 0xF8[7:0] + 0xF9[7:4]
    *adc_P = ((int32_t)buf[0] << 12) | ((int32_t)buf[1] << 4) | ((int32_t)buf[2] >> 4);

    // Temperature: 0xFA[7:0] + 0xFB[7:0] + 0xFC[7:4]
    *adc_T = ((int32_t)buf[3] << 12) | ((int32_t)buf[4] << 4) | ((int32_t)buf[5] >> 4);

    // Humidity: 0xFD[7:0] + 0xFE[7:0]
    *adc_H = ((int32_t)buf[6] << 8)  |  (int32_t)buf[7];
}

static uint8_t BME280_ReadCalibrationData(BME280_Handle *dev)
{
    uint8_t err_num = 0;
    HAL_StatusTypeDef status;
    uint8_t buf[24];

    status = BME280_ReadRegisters(dev, BME280_CALIB_TP_BASE, buf, 24);
    err_num += (status != HAL_OK);

    dev->dig_T1 = (uint16_t)(buf[1]  << 8) | buf[0];
    dev->dig_T2 =  (int16_t)(buf[3]  << 8) | buf[2];
    dev->dig_T3 =  (int16_t)(buf[5]  << 8) | buf[4];
    dev->dig_P1 = (uint16_t)(buf[7]  << 8) | buf[6];
    dev->dig_P2 =  (int16_t)(buf[9]  << 8) | buf[8];
    dev->dig_P3 =  (int16_t)(buf[11] << 8) | buf[10];
    dev->dig_P4 =  (int16_t)(buf[13] << 8) | buf[12];
    dev->dig_P5 =  (int16_t)(buf[15] << 8) | buf[14];
    dev->dig_P6 =  (int16_t)(buf[17] << 8) | buf[16];
    dev->dig_P7 =  (int16_t)(buf[19] << 8) | buf[18];
    dev->dig_P8 =  (int16_t)(buf[21] << 8) | buf[20];
    dev->dig_P9 =  (int16_t)(buf[23] << 8) | buf[22];

    status = BME280_ReadRegisters(dev, BME280_CALIB_H1_BASE, buf, 1);
    err_num += (status != HAL_OK);
    dev->dig_H1 = buf[0];

    status = BME280_ReadRegisters(dev, BME280_CALIB_H2_BASE, buf, 7);
    err_num += (status != HAL_OK);
    dev->dig_H2 =  (int16_t)(buf[1] << 8) | buf[0];
    dev->dig_H3 = buf[2];
    dev->dig_H4 =  (int16_t)(buf[3] << 4) | (buf[4] & 0x0F); // 0xE4[7:0] + 0xE5[3:0]
    dev->dig_H5 =  (int16_t)(buf[5] << 4) | (buf[4] >> 4);   // 0xE6[7:0] + 0xE5[7:4]
    dev->dig_H6 =   (int8_t)buf[6];

    return err_num;
}

// TEMPERATURE COMPENSATION
// Returns temperature in 0.01 degrees C (e.g. 2345 = 23.45C)
int32_t BME280_CompensateTemperature(BME280_Handle *dev, int32_t adc_T)
{
    int32_t var1, var2, T;

    var1 = ((((adc_T >> 3) - ((int32_t)dev->dig_T1 << 1))) *
            ((int32_t)dev->dig_T2)) >> 11;

    var2 = (((((adc_T >> 4) - ((int32_t)dev->dig_T1)) *
              ((adc_T >> 4) - ((int32_t)dev->dig_T1))) >> 12) *
            ((int32_t)dev->dig_T3)) >> 14;

    dev->t_fine = var1 + var2;
    T = (dev->t_fine * 5 + 128) >> 8;

    return T;
}

// PRESSURE COMPENSATION
// Returns pressure in Pa as Q24.8 fixed point
uint32_t BME280_CompensatePressure(BME280_Handle *dev, int32_t adc_P)
{
    int32_t var1, var2;
    uint32_t p;

    var1 = (((int32_t)dev->t_fine) >> 1) - (int32_t)64000;
    var2 = (((var1 >> 2) * (var1 >> 2)) >> 11) * ((int32_t)dev->dig_P6);
    var2 = var2 + ((var1 * ((int32_t)dev->dig_P5)) << 1);
    var2 = (var2 >> 2) + (((int32_t)dev->dig_P4) << 16);
    var1 = (((dev->dig_P3 * (((var1 >> 2) * (var1 >> 2)) >> 13)) >> 3) +
            ((((int32_t)dev->dig_P2) * var1) >> 1)) >> 18;
    var1 = ((((32768 + var1)) * ((int32_t)dev->dig_P1)) >> 15);

    if (var1 == 0) return 0;   // avoid division by zero

    p = (((uint32_t)(((int32_t)1048576) - adc_P) - (var2 >> 12))) * 3125;

    if (p < 0x80000000)
        p = (p << 1) / ((uint32_t)var1);
    else
        p = (p / (uint32_t)var1) * 2;

    var1 = (((int32_t)dev->dig_P9) * ((int32_t)(((p >> 3) * (p >> 3)) >> 13))) >> 12;
    var2 = (((int32_t)(p >> 2)) * ((int32_t)dev->dig_P8)) >> 13;

    p = (uint32_t)((int32_t)p + ((var1 + var2 + dev->dig_P7) >> 4));

    return p;
}

// HUMIDITY COMPENSATION
// Returns humidity in %RH as Q22.10 fixed point
uint32_t BME280_CompensateHumidity(BME280_Handle *dev, int32_t adc_H)
{
    int32_t v_x1_u32r;

    v_x1_u32r = dev->t_fine - ((int32_t)76800);

    v_x1_u32r = (((((adc_H << 14) - (((int32_t)dev->dig_H4) << 20) -
                    (((int32_t)dev->dig_H5) * v_x1_u32r)) + ((int32_t)16384)) >> 15) *
                 (((((((v_x1_u32r * ((int32_t)dev->dig_H6)) >> 10) *
                      (((v_x1_u32r * ((int32_t)dev->dig_H3)) >> 11) + ((int32_t)32768))) >> 10) +
                    ((int32_t)2097152)) * ((int32_t)dev->dig_H2) + 8192) >> 14));

    v_x1_u32r = v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) *
                               ((int32_t)dev->dig_H1)) >> 4);

    // Clamp to valid range
    v_x1_u32r = (v_x1_u32r < 0) ? 0 : v_x1_u32r;
    v_x1_u32r = (v_x1_u32r > 419430400) ? 419430400 : v_x1_u32r;

    return (uint32_t)(v_x1_u32r >> 12);
}

HAL_StatusTypeDef BME280_ReadRegister(BME280_Handle *dev, uint8_t reg, uint8_t *data)
{
    return HAL_I2C_Mem_Read(dev->hi2c, dev->i2c_addr, reg, I2C_MEMADD_SIZE_8BIT, data, 1, HAL_MAX_DELAY);
}

HAL_StatusTypeDef BME280_ReadRegisters(BME280_Handle *dev, uint8_t reg, uint8_t *data, uint8_t len)
{
    return HAL_I2C_Mem_Read(dev->hi2c, dev->i2c_addr, reg, I2C_MEMADD_SIZE_8BIT, data, len, HAL_MAX_DELAY);
}

HAL_StatusTypeDef BME280_WriteRegister(BME280_Handle *dev, uint8_t reg, uint8_t *data)
{
    return HAL_I2C_Mem_Write(dev->hi2c, dev->i2c_addr, reg, I2C_MEMADD_SIZE_8BIT, data, 1, HAL_MAX_DELAY);
}
