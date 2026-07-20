#ifndef MPU6050_H
#define MPU6050_H

#include "main.h"   /* pulls in I2C_HandleTypeDef via stm32f4xx_hal.h */
#include <stdint.h>
#include <stdbool.h>

/* 7-bit I2C address 0x68, shifted for HAL's 8-bit address convention.
 * If your MPU6050's AD0 pin is tied HIGH instead of GND/floating,
 * the address becomes 0x69 -> change to (0x69 << 1). */
#define MPU6050_I2C_ADDR   (0x68 << 1)

/* Register map (only what this driver uses) */
#define MPU6050_REG_WHO_AM_I      0x75
#define MPU6050_REG_PWR_MGMT_1    0x6B
#define MPU6050_REG_ACCEL_CONFIG  0x1C
#define MPU6050_REG_ACCEL_XOUT_H  0x3B

#define MPU6050_WHO_AM_I_VALUE    0x68

/* Accel full-scale range: using +/-2g (most sensitive, matches the
 * 16384 LSB/g scale factor used in MPU6050_Read_Accel_g below).
 * If you need a wider range for stronger vibration, change
 * ACCEL_CONFIG's value here AND the scale factor in the .c file
 * together - they must match. */
#define MPU6050_ACCEL_FS_2G       0x00

/* The I2C handle this driver uses. Declared here, defined by
 * CubeMX-generated code (in main.c or i2c.c) once I2C1 is enabled.
 * If your bus is I2C2/I2C3 instead, change every "hi2c1" in
 * mpu6050.c to match. */
extern I2C_HandleTypeDef hi2c1;

/* Initializes the MPU6050: verifies WHO_AM_I, wakes it from sleep
 * mode, and sets the accel range. Returns true on success, false if
 * the device didn't respond or WHO_AM_I didn't match (check wiring/
 * address if this fails). */
bool MPU6050_Init(void);

/* Reads the three accelerometer axes and converts to g's.
 * Blocking I2C call (HAL_MAX_DELAY). */
void MPU6050_Read_Accel_g(float *ax, float *ay, float *az);

#endif /* MPU6050_H */
