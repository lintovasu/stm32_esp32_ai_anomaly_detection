################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../bootloader/Src/crc32.c \
../bootloader/Src/flash_ll.c \
../bootloader/Src/flash_metadata.c 

OBJS += \
./bootloader/Src/crc32.o \
./bootloader/Src/flash_ll.o \
./bootloader/Src/flash_metadata.o 

C_DEPS += \
./bootloader/Src/crc32.d \
./bootloader/Src/flash_ll.d \
./bootloader/Src/flash_metadata.d 


# Each subdirectory must supply rules for building sources it contributes
bootloader/Src/%.o bootloader/Src/%.su bootloader/Src/%.cyclo: ../bootloader/Src/%.c bootloader/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F407xx -c -I../Core/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../Middlewares/ST/AI/Inc -I"C:/stm32espai_anomaly_detection/stm32_esp32_ai_anomaly_detection/stm32_sensor_sim_ai_anomalydetection/bootloader/Inc" -I"C:/stm32espai_anomaly_detection/stm32_esp32_ai_anomaly_detection/stm32_sensor_sim_ai_anomalydetection/app_ota_client/Inc" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-bootloader-2f-Src

clean-bootloader-2f-Src:
	-$(RM) ./bootloader/Src/crc32.cyclo ./bootloader/Src/crc32.d ./bootloader/Src/crc32.o ./bootloader/Src/crc32.su ./bootloader/Src/flash_ll.cyclo ./bootloader/Src/flash_ll.d ./bootloader/Src/flash_ll.o ./bootloader/Src/flash_ll.su ./bootloader/Src/flash_metadata.cyclo ./bootloader/Src/flash_metadata.d ./bootloader/Src/flash_metadata.o ./bootloader/Src/flash_metadata.su

.PHONY: clean-bootloader-2f-Src

