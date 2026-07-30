################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/ai_inference.c \
../Core/Src/main.c \
../Core/Src/mpu6050.c \
../Core/Src/network.c \
../Core/Src/network_data.c \
../Core/Src/sensor_protocol.c \
../Core/Src/stm32f4xx_hal_msp.c \
../Core/Src/stm32f4xx_it.c \
../Core/Src/syscalls.c \
../Core/Src/sysmem.c \
../Core/Src/system_stm32f4xx.c \
../Core/Src/vibration_itm_logger.c \
../Core/Src/vibration_rms.c 

OBJS += \
./Core/Src/ai_inference.o \
./Core/Src/main.o \
./Core/Src/mpu6050.o \
./Core/Src/network.o \
./Core/Src/network_data.o \
./Core/Src/sensor_protocol.o \
./Core/Src/stm32f4xx_hal_msp.o \
./Core/Src/stm32f4xx_it.o \
./Core/Src/syscalls.o \
./Core/Src/sysmem.o \
./Core/Src/system_stm32f4xx.o \
./Core/Src/vibration_itm_logger.o \
./Core/Src/vibration_rms.o 

C_DEPS += \
./Core/Src/ai_inference.d \
./Core/Src/main.d \
./Core/Src/mpu6050.d \
./Core/Src/network.d \
./Core/Src/network_data.d \
./Core/Src/sensor_protocol.d \
./Core/Src/stm32f4xx_hal_msp.d \
./Core/Src/stm32f4xx_it.d \
./Core/Src/syscalls.d \
./Core/Src/sysmem.d \
./Core/Src/system_stm32f4xx.d \
./Core/Src/vibration_itm_logger.d \
./Core/Src/vibration_rms.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F407xx -c -I../Core/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../Middlewares/ST/AI/Inc -I"C:/stm32espai_anomaly_detection/stm32_esp32_ai_anomaly_detection/stm32_sensor_sim_ai_anomalydetection/bootloader/Inc" -I"C:/stm32espai_anomaly_detection/stm32_esp32_ai_anomaly_detection/stm32_sensor_sim_ai_anomalydetection/app_ota_client/Inc" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/ai_inference.cyclo ./Core/Src/ai_inference.d ./Core/Src/ai_inference.o ./Core/Src/ai_inference.su ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/mpu6050.cyclo ./Core/Src/mpu6050.d ./Core/Src/mpu6050.o ./Core/Src/mpu6050.su ./Core/Src/network.cyclo ./Core/Src/network.d ./Core/Src/network.o ./Core/Src/network.su ./Core/Src/network_data.cyclo ./Core/Src/network_data.d ./Core/Src/network_data.o ./Core/Src/network_data.su ./Core/Src/sensor_protocol.cyclo ./Core/Src/sensor_protocol.d ./Core/Src/sensor_protocol.o ./Core/Src/sensor_protocol.su ./Core/Src/stm32f4xx_hal_msp.cyclo ./Core/Src/stm32f4xx_hal_msp.d ./Core/Src/stm32f4xx_hal_msp.o ./Core/Src/stm32f4xx_hal_msp.su ./Core/Src/stm32f4xx_it.cyclo ./Core/Src/stm32f4xx_it.d ./Core/Src/stm32f4xx_it.o ./Core/Src/stm32f4xx_it.su ./Core/Src/syscalls.cyclo ./Core/Src/syscalls.d ./Core/Src/syscalls.o ./Core/Src/syscalls.su ./Core/Src/sysmem.cyclo ./Core/Src/sysmem.d ./Core/Src/sysmem.o ./Core/Src/sysmem.su ./Core/Src/system_stm32f4xx.cyclo ./Core/Src/system_stm32f4xx.d ./Core/Src/system_stm32f4xx.o ./Core/Src/system_stm32f4xx.su ./Core/Src/vibration_itm_logger.cyclo ./Core/Src/vibration_itm_logger.d ./Core/Src/vibration_itm_logger.o ./Core/Src/vibration_itm_logger.su ./Core/Src/vibration_rms.cyclo ./Core/Src/vibration_rms.d ./Core/Src/vibration_rms.o ./Core/Src/vibration_rms.su

.PHONY: clean-Core-2f-Src

