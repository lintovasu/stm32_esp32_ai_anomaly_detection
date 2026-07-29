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
ROOT_DIR = stm32_sensor_sim_ai_anomalydetection
CUBE_DIR = $(ROOT_DIR)/Drivers

CMSIS_INC = $(CUBE_DIR)/CMSIS/Include
CMSIS_DEV_INC = $(CUBE_DIR)/CMSIS/Device/ST/STM32F4xx/Include
HAL_INC = $(CUBE_DIR)/STM32F4xx_HAL_Driver/Inc
BOOT_LOADER_INC =$(ROOT_DIR)/bootloader/Inc
MIDDLEWARE_INC = $(ROOT_DIR)/Middlewares/ST/AI/Inc
OTA_CLIENT_INC =$(ROOT_DIR)/app_ota_client/Inc

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

APP_SRCS := $(wildcard $(ROOT_DIR)/Core/Src/*.c)
OTA_CLIENT_SRCS := $(wildcard $(ROOT_DIR)/app_ota_client/Src/*.c)
#BOOT_LOADER_SRC := $(wildcard $(ROOT_DIR)/bootloader/Src/*.c)
HAL_SRC = $(CUBE_DIR)/STM32F4xx_HAL_Driver/Src
STARTUP :=$(wildcard $(ROOT_DIR)/Core/Startup/*.s)
HAL_SRCS = $(wildcard $(HAL_SRC)/*.c)
BOOT_LOADER_SRC = \
  $(ROOT_DIR)/bootloader/Src/flash_metadata.c \
  $(ROOT_DIR)/bootloader/Src/flash_ll.c \
  $(ROOT_DIR)/bootloader/Src/crc32.c

SRCS = $(APP_SRCS) $(OTA_CLIENT_SRCS) $(BOOT_LOADER_SRC) $(HAL_SRCS) $(STARTUP)

INC = \
  -Iapp_ota_client/inc \
  -Ibootloader/inc \
  -IInc -I$(ROOT_DIR)/Core/Inc \
  -I$(CMSIS_INC) \
  -I$(CMSIS_DEV_INC) \
  -I$(HAL_INC) \
  -I$(OTA_CLIENT_INC) \
  -I$(BOOT_LOADER_INC) \
  -I$(MIDDLEWARE_INC) \
  -Iconfig

DEFS = -DSTM32F407xx -DUSE_HAL_DRIVER -DFW_VERSION=\"$(VERSION)\" -DFW_SLOT=\"$(SLOT)\"

CFLAGS = $(MCU_FLAGS) -std=gnu11 $(DEFS) $(INC) -Wall -O2 -ffunction-sections -fdata-sections -g

# AI_LIB_FILE is passed directly as a link input (like an object file),
# not via -L/-l -- see note above on why -l doesn't work for this
# particular filename.
LDFLAGS = $(MCU_FLAGS) -T$(LDSCRIPT) -Wl,--gc-sections -specs=nano.specs -specs=nosys.specs \
  -L$(AI_LIB_DIR) -l$(AI_LIB_NAME)

.PHONY: all clean

OBJS := $(addprefix $(BUILD_DIR)/,$(notdir $(SRCS:.c=.o)))
OBJS := $(patsubst %.s.o,%.o,$(OBJS:.s=.o))

vpath %.c $(sort $(dir $(SRCS)))
vpath %.s $(sort $(dir $(SRCS)))

all: $(BUILD_DIR)/$(TARGET).bin

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.s | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Link order matters here: OBJS first (they're what REFERENCE the
# forward_lite_*/stai_network_run symbols), then AI_LIB_FILE (which
# PROVIDES them), then -lm last. This mirrors the order your working
# CubeIDE build used.
$(BUILD_DIR)/$(TARGET).elf: $(OBJS)
	$(CC) $(MCU_FLAGS) $(OBJS) $(LDFLAGS) -lm -o $@
	$(SIZE) $@

$(BUILD_DIR)/$(TARGET).bin: $(BUILD_DIR)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@
	@MAXSIZE=$$( [ "$(SLOT)" = "A" ] && echo 458752 || echo 524288 ); \
	ACTUAL=$$(stat -c%s $@ 2>/dev/null || stat -f%z $@); \
	echo "Slot $(SLOT) image: $$ACTUAL bytes (limit $$MAXSIZE)"; \
	[ $$ACTUAL -le $$MAXSIZE ] || (echo "ERROR: firmware exceeds Slot $(SLOT) capacity!" && exit 1)

clean:
	rm -rf $(BUILD_DIR)
