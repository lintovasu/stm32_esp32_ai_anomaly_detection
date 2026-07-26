# Makefile for the application firmware -- builds for whichever slot you
# pass in, e.g.:
#   make -f ci/Makefile.app SLOT=A VERSION=1.4.0
#   make -f ci/Makefile.app SLOT=B VERSION=1.4.0
#
# Same source tree, only the linker script (and hence the flash origin)
# differs between the two invocations. This is the one Makefile CI calls
# twice per release.

SLOT ?= A
VERSION ?= 0.0.0-dev

ifeq ($(SLOT),A)
  LDSCRIPT = stm32_sensor_sim_ai_anomalydetection/linker/app_slotA.ld
else ifeq ($(SLOT),B)
  LDSCRIPT = stm32_sensor_sim_ai_anomalydetection/linker/app_slotB.ld
else
  $(error SLOT must be A or B, got '$(SLOT)')
endif

TARGET      = firmware_v$(VERSION)_slot$(SLOT)
BUILD_DIR   = build/app_slot$(SLOT)

CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE    = arm-none-eabi-size

MCU_FLAGS = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
ROOT_DIR = .
CUBE_DIR = Drivers
CMSIS_INC = $(CUBE_DIR)/CMSIS/Include
CMSIS_DEV_INC = $(CUBE_DIR)/CMSIS/Device/ST/STM32F4xx/Include
HAL_INC = $(CUBE_DIR)/STM32F4xx_HAL_Driver/Inc
HAL_SRC = $(CUBE_DIR)/STM32F4xx_HAL_Driver/Src
MIDDLEWARE_INC = $(ROOT_DIR)/Middlewares/ST/AI/Inc
#MIDDLEWARE_MISC_INC = $(ROOT_DIR)/Middlewares/ST/AI/MISC/Inc

# X-CUBE-AI runtime library -- the .a file CubeIDE links in automatically
# via project settings, which a plain Makefile has to be told about
# explicitly. Find the real filename with:
#     find . -iname "*NetworkRuntime*.a"
# or check Project Properties > C/C++ Build > Settings > MCU GCC Linker
# > Libraries in CubeIDE, then set AI_LIB_DIR / AI_LIB_NAME to match
# (AI_LIB_NAME is the filename minus the "lib" prefix and ".a" suffix).
AI_LIB_DIR = stm32_sensor_sim_ai_anomalydetection
AI_LIB_NAME ?= Net1201

# Your existing application sources -- replace this glob with your actual
# project's file list (sensor drivers, FreeRTOS, MQTT/UART handling,
# etc). Shown here as a placeholder pattern plus the OTA client module
# and shared flash/crc/metadata code every app build needs.

APP_SRCS := $(wildcard Src/*.c) $(wildcard Core/Src/*.c)
OTA_CLIENT_SRCS := $(wildcard app_ota_client/Src/*.c)
STARTUP :=$(wildcard Core/Startup/*.s)
#OTA_CLIENT_SRCS = \
#  app_ota_client/Src/ota_receiver.c \
#  app_ota_client/Src/ota_confirm.c \
#  bootloader/Src/flash_metadata.c \
#  bootloader/Src/flash_ll.c \
#  bootloader/Src/crc32.c

HAL_SRCS = $(wildcard $(HAL_SRC)/*.c)

SRCS = $(APP_SRCS) $(OTA_CLIENT_SRCS) $(HAL_SRCS) $(STARTUP)

INC = \
  -Iapp_ota_client/inc \
  -Ibootloader/inc \
  -IInc -ICore/Inc \
  -I$(CMSIS_INC) \
  -I$(CMSIS_DEV_INC) \
  -I$(HAL_INC) \
  -I$(MIDDLEWARE_INC) \
  -Iconfig

DEFS = -DSTM32F407xx -DUSE_HAL_DRIVER -DFW_VERSION=\"$(VERSION)\" -DFW_SLOT=\"$(SLOT)\"

CFLAGS = $(MCU_FLAGS) $(DEFS) $(INC) -Wall -O2 -ffunction-sections -fdata-sections -g

# -L points the linker at the folder containing the .a file; -l links it
# by name. -lm links the math library for sinf/sqrtf/etc used in
# vibration_rms.c and elsewhere. Order matters less with modern ld, but
# keep -lm last as a general habit (libraries are searched left to right
# for unresolved symbols).
LDFLAGS = $(MCU_FLAGS) -T$(LDSCRIPT) -Wl,--gc-sections -specs=nano.specs -specs=nosys.specs \
  -L$(AI_LIB_DIR) -l$(AI_LIB_NAME) \
  -lm

.PHONY: all clean

all: $(BUILD_DIR)/$(TARGET).bin

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/$(TARGET).elf: $(SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SRCS) $(LDFLAGS) -o $@
	$(SIZE) $@

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@
	@MAXSIZE=$$( [ "$(SLOT)" = "A" ] && echo 458752 || echo 524288 ); \
	ACTUAL=$$(stat -c%s $@ 2>/dev/null || stat -f%z $@); \
	echo "Slot $(SLOT) image: $$ACTUAL bytes (limit $$MAXSIZE)"; \
	[ $$ACTUAL -le $$MAXSIZE ] || (echo "ERROR: firmware exceeds Slot $(SLOT) capacity!" && exit 1)

clean:
	rm -rf $(BUILD_DIR)
