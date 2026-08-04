################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../app_ota_client/Src/ota_confirm.c \
../app_ota_client/Src/ota_receiver.c 

OBJS += \
./app_ota_client/Src/ota_confirm.o \
./app_ota_client/Src/ota_receiver.o 

C_DEPS += \
./app_ota_client/Src/ota_confirm.d \
./app_ota_client/Src/ota_receiver.d 


# Each subdirectory must supply rules for building sources it contributes
app_ota_client/Src/%.o app_ota_client/Src/%.su app_ota_client/Src/%.cyclo: ../app_ota_client/Src/%.c app_ota_client/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F407xx -c -I../Core/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc -I../Drivers/STM32F4xx_HAL_Driver/Inc/Legacy -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I../Middlewares/ST/AI/Inc -I"C:/stm32espai_anomaly_detection/stm32_esp32_ai_anomaly_detection/stm32_sensor_sim_ai_anomalydetection/bootloader/Inc" -I"C:/stm32espai_anomaly_detection/stm32_esp32_ai_anomaly_detection/stm32_sensor_sim_ai_anomalydetection/app_ota_client/Inc" -I"C:/stm32espai_anomaly_detection/stm32_esp32_ai_anomaly_detection/stm32_sensor_sim_ai_anomalydetection/Middlewares/Third_Party/FreeRTOS/Source/include" -I"C:/stm32espai_anomaly_detection/stm32_esp32_ai_anomaly_detection/stm32_sensor_sim_ai_anomalydetection/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2" -I"C:/stm32espai_anomaly_detection/stm32_esp32_ai_anomaly_detection/stm32_sensor_sim_ai_anomalydetection/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F" -I"C:/stm32espai_anomaly_detection/stm32_esp32_ai_anomaly_detection/stm32_sensor_sim_ai_anomalydetection/Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang" -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-app_ota_client-2f-Src

clean-app_ota_client-2f-Src:
	-$(RM) ./app_ota_client/Src/ota_confirm.cyclo ./app_ota_client/Src/ota_confirm.d ./app_ota_client/Src/ota_confirm.o ./app_ota_client/Src/ota_confirm.su ./app_ota_client/Src/ota_receiver.cyclo ./app_ota_client/Src/ota_receiver.d ./app_ota_client/Src/ota_receiver.o ./app_ota_client/Src/ota_receiver.su

.PHONY: clean-app_ota_client-2f-Src

