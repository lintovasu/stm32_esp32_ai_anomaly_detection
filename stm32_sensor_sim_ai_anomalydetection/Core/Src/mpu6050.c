#include "mpu6050.h"

bool MPU6050_Init(void)
{
    uint8_t who_am_i = 0;
    HAL_StatusTypeDef status;

    /* Confirm the device is actually on the bus and responding before
     * trying to configure it - catches wiring/address problems early
     * instead of silently reading garbage later. */
    status = HAL_I2C_IsDeviceReady(&hi2c1, MPU6050_I2C_ADDR, 3, 100);
    if (status != HAL_OK) {
        return false;
    }

    status = HAL_I2C_Mem_Read(&hi2c1, MPU6050_I2C_ADDR,
                               MPU6050_REG_WHO_AM_I, I2C_MEMADD_SIZE_8BIT,
                               &who_am_i, 1, 100);
    if (status != HAL_OK || who_am_i != MPU6050_WHO_AM_I_VALUE) {
        return false;
    }

    /* Wake the MPU6050 up - it powers on in sleep mode (bit 6 of
     * PWR_MGMT_1 set). Writing 0x00 clears sleep and selects the
     * internal 8MHz oscillator as clock source. */
    uint8_t pwr_mgmt_1 = 0x00;
    status = HAL_I2C_Mem_Write(&hi2c1, MPU6050_I2C_ADDR,
                                MPU6050_REG_PWR_MGMT_1, I2C_MEMADD_SIZE_8BIT,
                                &pwr_mgmt_1, 1, 100);
    if (status != HAL_OK) {
        return false;
    }

    /* Set accelerometer full-scale range to +/-2g. */
    uint8_t accel_config = MPU6050_ACCEL_FS_2G;
    status = HAL_I2C_Mem_Write(&hi2c1, MPU6050_I2C_ADDR,
                                MPU6050_REG_ACCEL_CONFIG, I2C_MEMADD_SIZE_8BIT,
                                &accel_config, 1, 100);
    if (status != HAL_OK) {
        return false;
    }

    return true;
}

void MPU6050_Read_Accel_g(float *ax, float *ay, float *az)
{
    uint8_t raw[6];

    /* ACCEL_XOUT_H..ACCEL_ZOUT_L are 6 consecutive registers -
     * one burst read instead of three separate transactions. */
    HAL_I2C_Mem_Read(&hi2c1, MPU6050_I2C_ADDR,
                      MPU6050_REG_ACCEL_XOUT_H, I2C_MEMADD_SIZE_8BIT,
                      raw, 6, HAL_MAX_DELAY);

    /* Each axis is 16-bit, big-endian, two's complement. */
    int16_t raw_x = (int16_t)((raw[0] << 8) | raw[1]);
    int16_t raw_y = (int16_t)((raw[2] << 8) | raw[3]);
    int16_t raw_z = (int16_t)((raw[4] << 8) | raw[5]);

    /* Scale factor for +/-2g range: 16384 LSB per g (from the
     * MPU6050 datasheet). This MUST match MPU6050_ACCEL_FS_2G above -
     * if you change the range, change this scale factor too. */
    const float scale = 16384.0f;

    *ax = (float)raw_x / scale;
    *ay = (float)raw_y / scale;
    *az = (float)raw_z / scale;
}
